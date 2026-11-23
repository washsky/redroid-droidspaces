import android.media.*;
import android.opengl.*;
import android.os.*;
import android.view.Surface;
import java.io.*;
import java.nio.*;
public class SurfaceCodecProbe {
 static volatile boolean done;static native int forceLinear(Surface surface);
 static void report(String s){System.out.println(s);android.util.Log.i("ARM13SURFACE",s);}
 public static void main(String[] args) throws Exception {
  Looper.prepareMainLooper();
  new Thread(()->{SystemClock.sleep(22000);if(!done){report("WATCHDOG_TIMEOUT");android.os.Process.killProcess(android.os.Process.myPid());}},"Surface-Probe-Deadline").start();
  String name=args.length>0?args[0]:"c2.qti.avc.encoder";boolean canvasMode=java.util.Arrays.asList(args).contains("--canvas");
  MediaCodec codec=null;Surface surface=null;EGLDisplay display=null;EGLContext context=null;EGLSurface window=null;MediaMuxer mux=null;boolean started=false,muxStarted=false;
  try {
   codec=MediaCodec.createByCodecName(name);MediaFormat format=MediaFormat.createVideoFormat("video/avc",320,240);
   format.setInteger(MediaFormat.KEY_COLOR_FORMAT,MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
   format.setInteger(MediaFormat.KEY_BIT_RATE,600000);format.setInteger(MediaFormat.KEY_FRAME_RATE,15);format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL,1);
   if(args.length>1&&!args[1].startsWith("--")){format.setInteger("vendor.qti-ext-enc-linear-color-format.value",Integer.parseInt(args[1]));report("LINEAR_PARAM="+args[1]);}report("CONFIGURE "+name);codec.configure(format,null,null,MediaCodec.CONFIGURE_FLAG_ENCODE);surface=codec.createInputSurface();if(java.util.Arrays.asList(args).contains("--linear")){System.load("/data/local/tmp/libarm13_linear_window.so");report("LINEAR_BEFORE="+forceLinear(surface));}report("INPUT_SURFACE_READY");
   if(!canvasMode){display=EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);int[] v=new int[2];if(!EGL14.eglInitialize(display,v,0,v,1))throw new Exception("eglInitialize 0x"+Integer.toHexString(EGL14.eglGetError()));
   int[] attr={EGL14.EGL_RED_SIZE,8,EGL14.EGL_GREEN_SIZE,8,EGL14.EGL_BLUE_SIZE,8,EGL14.EGL_RENDERABLE_TYPE,EGL14.EGL_OPENGL_ES2_BIT,0x3142,1,EGL14.EGL_NONE};
   EGLConfig[] configs=new EGLConfig[1];int[] count=new int[1];if(!EGL14.eglChooseConfig(display,attr,0,configs,0,1,count,0)||count[0]<1)throw new Exception("eglChooseConfig failed");
   context=EGL14.eglCreateContext(display,configs[0],EGL14.EGL_NO_CONTEXT,new int[]{EGL14.EGL_CONTEXT_CLIENT_VERSION,2,EGL14.EGL_NONE},0);
   window=EGL14.eglCreateWindowSurface(display,configs[0],surface,new int[]{EGL14.EGL_NONE},0);
   if(window==EGL14.EGL_NO_SURFACE||!EGL14.eglMakeCurrent(display,window,window,context))throw new Exception("EGL window 0x"+Integer.toHexString(EGL14.eglGetError()));
   if(java.util.Arrays.asList(args).contains("--linear"))report("LINEAR_AFTER="+forceLinear(surface));report("RENDERER="+GLES20.glGetString(GLES20.GL_RENDERER));}codec.start();started=true;report("CODEC_STARTED INPUT="+codec.getInputFormat());
   String path="/data/user/0/lab.arm13.gpuprobe/files/surface-"+name+".mp4";mux=new MediaMuxer(path,MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
   int submitted=0,encoded=0,track=-1;boolean eosIn=false,eosOut=false;long deadline=SystemClock.elapsedRealtime()+15000;MediaCodec.BufferInfo info=new MediaCodec.BufferInfo();
   while(!eosOut&&SystemClock.elapsedRealtime()<deadline){
    if(submitted<30){if(canvasMode){android.graphics.Canvas c=surface.lockCanvas(null);c.drawColor(android.graphics.Color.rgb((submitted%10)*255/9,77,179));surface.unlockCanvasAndPost(c);}else{GLES20.glClearColor((submitted%10)/9f,.3f,.7f,1);GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);if(submitted==0||submitted==29){ByteBuffer px=ByteBuffer.allocateDirect(4);GLES20.glReadPixels(160,120,1,1,GLES20.GL_RGBA,GLES20.GL_UNSIGNED_BYTE,px);report("GPU_PIXEL frame="+submitted+" rgb="+(px.get(0)&255)+","+(px.get(1)&255)+","+(px.get(2)&255)+" error="+GLES20.glGetError());}EGLExt.eglPresentationTimeANDROID(display,window,(132+submitted*1000000L/15)*1000);
     if(!EGL14.eglSwapBuffers(display,window))throw new Exception("eglSwapBuffers 0x"+Integer.toHexString(EGL14.eglGetError()));}submitted++;if(submitted==1)report("FIRST_SWAP_OK");
    }else if(!eosIn){codec.signalEndOfInputStream();eosIn=true;}
    int i=codec.dequeueOutputBuffer(info,10000);
    if(i==MediaCodec.INFO_OUTPUT_FORMAT_CHANGED){track=mux.addTrack(codec.getOutputFormat());mux.start();muxStarted=true;report("OUTPUT_FORMAT_READY");}
    else if(i>=0){if(info.size>0&&(info.flags&MediaCodec.BUFFER_FLAG_CODEC_CONFIG)==0){ByteBuffer b=codec.getOutputBuffer(i);b.position(info.offset);b.limit(info.offset+info.size);mux.writeSampleData(track,b,info);encoded++;}eosOut=(info.flags&MediaCodec.BUFFER_FLAG_END_OF_STREAM)!=0;codec.releaseOutputBuffer(i,false);}
   }
   report("SURFACE_RESULT CODEC="+name+" SUBMITTED="+submitted+" ENCODED="+encoded+" EOS="+eosOut+" FILE="+path);
   if(encoded!=30||!eosOut)throw new Exception("No complete encoded stream");report("SURFACE_PASS");
  } catch(Throwable e){e.printStackTrace(System.out);report("SURFACE_FAIL="+e);}
  finally {
   if(display!=null&&context!=null){EGL14.eglMakeCurrent(display,EGL14.EGL_NO_SURFACE,EGL14.EGL_NO_SURFACE,EGL14.EGL_NO_CONTEXT);if(window!=null)EGL14.eglDestroySurface(display,window);EGL14.eglDestroyContext(display,context);EGL14.eglTerminate(display);}
   if(surface!=null)surface.release();if(codec!=null){if(started)try{codec.stop();}catch(Exception e){}codec.release();}if(mux!=null){if(muxStarted)try{mux.stop();}catch(Exception e){}mux.release();}done=true;System.exit(0);
  }
 }
}
