import android.media.*;import android.os.*;import android.view.*;import android.graphics.*;import java.nio.*;import java.lang.reflect.*;
public class ImageReaderProbe {
 static volatile boolean done;static Class<?> sc;static Object call(String n,Class<?>[] t,Object...a)throws Exception{Method m=sc.getDeclaredMethod(n,t);m.setAccessible(true);return m.invoke(null,a);}
 public static void main(String[] args)throws Exception{
  Looper.prepareMainLooper();new Thread(()->{SystemClock.sleep(12000);if(!done){System.out.println("READER_WATCHDOG");android.os.Process.killProcess(android.os.Process.myPid());}}).start();
  ImageReader reader=null;IBinder token=null;
  try{sc=Class.forName("android.view.SurfaceControl");reader=ImageReader.newInstance(360,640,PixelFormat.RGBA_8888,3);
   token=(IBinder)call("createDisplay",new Class[]{String.class,boolean.class},"ARM13 ImageReader controlled probe",false);
   call("openTransaction",new Class[]{});
   try{call("setDisplaySurface",new Class[]{IBinder.class,Surface.class},token,reader.getSurface());call("setDisplayProjection",new Class[]{IBinder.class,int.class,Rect.class,Rect.class},token,0,new Rect(0,0,720,1280),new Rect(0,0,360,640));call("setDisplayLayerStack",new Class[]{IBinder.class,int.class},token,0);}finally{call("closeTransaction",new Class[]{});}
   int count=0;long deadline=SystemClock.elapsedRealtime()+6000;
   while(count<30&&SystemClock.elapsedRealtime()<deadline){Image image=reader.acquireLatestImage();if(image==null){SystemClock.sleep(20);continue;}
    try{Image.Plane p=image.getPlanes()[0];ByteBuffer b=p.getBuffer();int x=180,y=320,index=b.position()+y*p.getRowStride()+x*p.getPixelStride();int r=b.get(index)&255,g=b.get(index+1)&255,blue=b.get(index+2)&255;if(count==0||count==29)System.out.println("READER_RGB frame="+count+" RGB="+r+","+g+","+blue+" STRIDE="+p.getRowStride());count++;}finally{image.close();}
   }
   System.out.println("READER_FRAMES="+count);
  }finally{if(token!=null)call("destroyDisplay",new Class[]{IBinder.class},token);if(reader!=null)reader.close();done=true;System.exit(0);}
 }
}
