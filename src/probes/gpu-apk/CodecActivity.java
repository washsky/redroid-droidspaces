package lab.arm13.gpuprobe;
import android.app.Activity;
import android.os.Bundle;
import android.os.SystemClock;
import android.media.*;
import android.util.Log;
import android.widget.TextView;
import android.graphics.ImageFormat;
import java.nio.ByteBuffer;
import java.io.File;
import java.io.FileOutputStream;
import android.graphics.Rect;
public final class CodecActivity extends Activity {
 TextView text; volatile boolean done;
 void report(String s){Log.i("ARM13CODEC",s);runOnUiThread(()->text.append(s+"\n"));}
 @Override public void onCreate(Bundle b){
  super.onCreate(b);text=new TextView(this);text.setTextSize(17);text.setPadding(20,32,20,20);setContentView(text);
  new Thread(()->{
   try {encode("video/avc","c2.qti.avc.encoder","h264-yuv.mp4");encode("video/hevc","c2.qti.hevc.encoder","hevc-yuv.mp4");report("ENCODE_ALL=PASS");decode("c2.qti.avc.decoder","h264-yuv.mp4","h264-hwdecoded.nv12");decode("c2.qti.hevc.decoder","hevc-yuv.mp4","hevc-hwdecoded.nv12");report("CODEC_ALL=PASS");}
   catch(Throwable e){Log.e("ARM13CODEC","CODEC_ALL=FAIL",e);report("CODEC_ALL=FAIL "+e);}
   finally{done=true;}
  },"YUV-Hardware-Codec").start();
  new Thread(()->{SystemClock.sleep(65000);if(!done){Log.e("ARM13CODEC","WATCHDOG_TIMEOUT");android.os.Process.killProcess(android.os.Process.myPid());}},"Codec-Test-Deadline").start();
 }
 void fill(Image image,int frame) {
  if(image==null)throw new IllegalStateException("No CPU writable YUV input Image");
  Image.Plane[] p=image.getPlanes();
  if(p.length!=3)throw new IllegalStateException("YUV planes="+p.length);
  for(int c=0;c<3;c++){
   ByteBuffer data=p[c].getBuffer();int base=data.position(),w=c==0?320:160,h=c==0?240:120;
   for(int y=0;y<h;y++)for(int x=0;x<w;x++){
    int value=c==0?16+(x*160/320+y*32/240+frame*3)%200:(c==1?96:160);
    data.put(base+y*p[c].getRowStride()+x*p[c].getPixelStride(),(byte)value);
   }
  }
 }
 void encode(String mime,String name,String file) throws Exception {
  final int w=320,h=240,frames=30;
  MediaCodec codec=MediaCodec.createByCodecName(name);
  File out=new File(getFilesDir(),file);MediaMuxer mux=null;boolean started=false;
  int encoded=0,submitted=0,track=-1;boolean eosIn=false,eosOut=false;
  try{
   MediaFormat format=MediaFormat.createVideoFormat(mime,w,h);
   format.setInteger(MediaFormat.KEY_COLOR_FORMAT,MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible);
   format.setInteger(MediaFormat.KEY_BIT_RATE,600000);format.setInteger(MediaFormat.KEY_FRAME_RATE,15);format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL,1);
   format.setInteger(MediaFormat.KEY_PROFILE,mime.equals("video/avc")?MediaCodecInfo.CodecProfileLevel.AVCProfileBaseline:MediaCodecInfo.CodecProfileLevel.HEVCProfileMain);
   report("ENCODER="+codec.getName()+" FORMAT="+format);
   codec.configure(format,null,null,MediaCodec.CONFIGURE_FLAG_ENCODE);codec.start();started=true;
   mux=new MediaMuxer(out.toString(),MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
   boolean muxStarted=false;long deadline=SystemClock.elapsedRealtime()+25000;
   MediaCodec.BufferInfo info=new MediaCodec.BufferInfo();
   while(!eosOut && SystemClock.elapsedRealtime()<deadline){
    if(!eosIn){
     int i=codec.dequeueInputBuffer(10000);
     if(i>=0){
      if(submitted<frames){
       Image image=codec.getInputImage(i);fill(image,submitted);image.close();
       codec.queueInputBuffer(i,0,w*h*3/2,132+submitted*1000000L/15,0);submitted++;
      }else{codec.queueInputBuffer(i,0,0,132+submitted*1000000L/15,MediaCodec.BUFFER_FLAG_END_OF_STREAM);eosIn=true;}
     }
    }
    int o=codec.dequeueOutputBuffer(info,10000);
    if(o==MediaCodec.INFO_OUTPUT_FORMAT_CHANGED){track=mux.addTrack(codec.getOutputFormat());mux.start();muxStarted=true;}
    else if(o>=0){
     ByteBuffer data=codec.getOutputBuffer(o);
     if((info.flags&MediaCodec.BUFFER_FLAG_CODEC_CONFIG)!=0)info.size=0;
     if(info.size>0){if(!muxStarted)throw new IllegalStateException("No output format");data.position(info.offset);data.limit(info.offset+info.size);mux.writeSampleData(track,data,info);encoded++;}
     eosOut=(info.flags&MediaCodec.BUFFER_FLAG_END_OF_STREAM)!=0;
     codec.releaseOutputBuffer(o,false);
    }
   }
   if(muxStarted){mux.stop();mux.release();mux=null;}
   if(encoded!=frames||!eosOut)throw new IllegalStateException(name+" encoded="+encoded+" submitted="+submitted+" eos="+eosOut);
   report("YUV_ENCODE=PASS CODEC="+name+" FRAMES="+encoded+" BYTES="+out.length()+" FILE="+out);
  }finally{
   if(mux!=null)try{mux.release();}catch(Exception e){}
   if(started)try{codec.stop();}catch(Exception e){}
   codec.release();
  }
 }

 void decode(String name,String file,String raw) throws Exception {
  MediaExtractor ex=new MediaExtractor();ex.setDataSource(new File(getFilesDir(),file).toString());ex.selectTrack(0);
  MediaFormat format=ex.getTrackFormat(0);
  format.setInteger(MediaFormat.KEY_COLOR_FORMAT,MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible);
  MediaCodec codec=MediaCodec.createByCodecName(name);boolean started=false;
  int decoded=0;boolean eosIn=false,eosOut=false;
  File out=new File(getFilesDir(),raw);
  try(FileOutputStream dump=new FileOutputStream(out)){
   report("DECODER="+codec.getName());
   codec.configure(format,null,null,0);codec.start();started=true;
   MediaCodec.BufferInfo info=new MediaCodec.BufferInfo();long deadline=SystemClock.elapsedRealtime()+25000;
   byte[] nv12=new byte[320*240*3/2];
   while(!eosOut&&SystemClock.elapsedRealtime()<deadline){
    if(!eosIn){
     int i=codec.dequeueInputBuffer(10000);
     if(i>=0){ByteBuffer input=codec.getInputBuffer(i);int size=ex.readSampleData(input,0);
      if(size<0){codec.queueInputBuffer(i,0,0,0,MediaCodec.BUFFER_FLAG_END_OF_STREAM);eosIn=true;}
      else{codec.queueInputBuffer(i,0,size,ex.getSampleTime(),0);ex.advance();}
     }
    }
    int o=codec.dequeueOutputBuffer(info,10000);
    if(o>=0){
     if(info.size>0){
      Image image=codec.getOutputImage(o);
      if(image==null)throw new IllegalStateException("No readable decoded YUV Image");
      Rect crop=image.getCropRect();if(crop.width()!=320||crop.height()!=240)throw new IllegalStateException("Unexpected crop "+crop);
      Image.Plane[] planes=image.getPlanes();if(planes.length!=3)throw new IllegalStateException("Decoder planes="+planes.length);
      for(int c=0;c<3;c++){
       ByteBuffer data=planes[c].getBuffer();int w=c==0?320:160,h=c==0?240:120;
       int x0=c==0?crop.left:crop.left/2,y0=c==0?crop.top:crop.top/2,base=data.position();
       for(int y=0;y<h;y++)for(int x=0;x<w;x++){
        byte v=data.get(base+(y0+y)*planes[c].getRowStride()+(x0+x)*planes[c].getPixelStride());
        int dst=c==0?y*320+x:320*240+y*320+x*2+(c-1);
        nv12[dst]=v;
       }
      }
      image.close();dump.write(nv12);decoded++;
     }
     eosOut=(info.flags&MediaCodec.BUFFER_FLAG_END_OF_STREAM)!=0;
     codec.releaseOutputBuffer(o,false);
    }
   }
   if(decoded!=30||!eosOut)throw new IllegalStateException(name+" decoded="+decoded+" eos="+eosOut);
   report("HW_DECODE=PASS CODEC="+name+" FRAMES="+decoded+" RAW="+out);
  }finally{if(started)try{codec.stop();}catch(Exception e){}codec.release();ex.release();}
 }

}
