package lab.arm13.gpuprobe;
import android.app.Activity;
import android.os.Bundle;
import android.opengl.GLSurfaceView;
import android.opengl.GLES20;
import android.hardware.HardwareBuffer;
import android.util.Log;
import java.nio.*;
import javax.microedition.khronos.opengles.GL10;
import javax.microedition.khronos.egl.EGLConfig;
public final class MainActivity extends Activity {
 static { System.loadLibrary("ahbprobe"); }
 static native int ahbRoundtrip();
 GLSurfaceView v;
 @Override public void onCreate(Bundle b) {
  super.onCreate(b); v=new GLSurfaceView(this); v.setEGLContextClientVersion(2);
  v.setRenderer(new GLSurfaceView.Renderer() {
   int program,w,h,n; FloatBuffer vertices;
   int shader(int type,String text) {
    int s=GLES20.glCreateShader(type);GLES20.glShaderSource(s,text);GLES20.glCompileShader(s);
    int[] ok=new int[1];GLES20.glGetShaderiv(s,GLES20.GL_COMPILE_STATUS,ok,0);
    if(ok[0]==0)throw new IllegalStateException(GLES20.glGetShaderInfoLog(s));return s;
   }
   public void onSurfaceCreated(GL10 gl,EGLConfig c) {
    Log.i("ARM13GPU","RENDERER="+GLES20.glGetString(GLES20.GL_RENDERER));
    program=GLES20.glCreateProgram();
    GLES20.glAttachShader(program,shader(GLES20.GL_VERTEX_SHADER,"attribute vec2 p; varying vec2 uv; void main(){uv=(p+1.0)*0.5;gl_Position=vec4(p,0,1);}"));
    GLES20.glAttachShader(program,shader(GLES20.GL_FRAGMENT_SHADER,"precision mediump float;varying vec2 uv;void main(){gl_FragColor=vec4(uv,0.25,1.0);}"));
    GLES20.glBindAttribLocation(program,0,"p");GLES20.glLinkProgram(program);
    int[] ok=new int[1];GLES20.glGetProgramiv(program,GLES20.GL_LINK_STATUS,ok,0);
    if(ok[0]==0)throw new IllegalStateException(GLES20.glGetProgramInfoLog(program));
    vertices=ByteBuffer.allocateDirect(24).order(ByteOrder.nativeOrder()).asFloatBuffer();
    vertices.put(new float[]{-1,-1,3,-1,-1,3}).position(0);
   }
   public void onSurfaceChanged(GL10 gl,int x,int y){w=x;h=y;GLES20.glViewport(0,0,w,h);}
   public void onDrawFrame(GL10 gl) {
    GLES20.glUseProgram(program);GLES20.glVertexAttribPointer(0,2,GLES20.GL_FLOAT,false,0,vertices);GLES20.glEnableVertexAttribArray(0);GLES20.glDrawArrays(GLES20.GL_TRIANGLES,0,3);
    if(++n==120) {
     ByteBuffer a=ByteBuffer.allocateDirect(4),b=ByteBuffer.allocateDirect(4);
     GLES20.glReadPixels(w/4,h/4,1,1,GLES20.GL_RGBA,GLES20.GL_UNSIGNED_BYTE,a);
     GLES20.glReadPixels(w*3/4,h*3/4,1,1,GLES20.GL_RGBA,GLES20.GL_UNSIGNED_BYTE,b);
     int r=a.get(0)&255,g=a.get(1)&255,r2=b.get(0)&255,g2=b.get(1)&255;
     boolean ok=Math.abs(r-64)<5&&Math.abs(g-64)<5&&Math.abs(r2-191)<5&&Math.abs(g2-191)<5&&GLES20.glGetError()==0;
     Log.i("ARM13GPU","GPU_DRAW="+(ok?"PASS":"FAIL")+" FRAMES="+n+" SIZE="+w+"x"+h+" PIXELS="+r+","+g+";"+r2+","+g2);
     v.post(()->v.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY));
    }
   }
  });
  setContentView(v);
  new Thread(()->{
   int r=ahbRoundtrip();
   for(int i=0;i<128;i++)try(HardwareBuffer hb=HardwareBuffer.create(64,64,HardwareBuffer.RGBA_8888,1,HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE|HardwareBuffer.USAGE_GPU_COLOR_OUTPUT)){}
   Log.i("ARM13GPU","APP_AHB_IMPORT_RELEASE="+(r==0?"PASS":"FAIL")+" JAVA_CLOSE=128");
  },"AHB-Acceptance").start();
 }
 @Override protected void onPause(){super.onPause();v.onPause();}
 @Override protected void onResume(){super.onResume();v.onResume();}
}