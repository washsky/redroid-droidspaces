import java.net.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import javax.net.ssl.*;
public class NetProbe {
 public static void main(String[] args) throws Exception {
  System.out.println("UID="+android.os.Process.myUid());
  System.out.println("DNS="+java.util.Arrays.toString(InetAddress.getAllByName("www.baidu.com")));
  URL u=new URL(args.length>0?args[0]:"https://www.baidu.com/");
  HttpsURLConnection c=(HttpsURLConnection)u.openConnection(Proxy.NO_PROXY);
  c.setConnectTimeout(8000);c.setReadTimeout(8000);
  System.out.println("HTTPS_CODE="+c.getResponseCode()+" CIPHER="+c.getCipherSuite());
  byte[] b=new byte[160];int n=c.getInputStream().read(b);
  System.out.println("BODY="+new String(b,0,Math.max(n,0),StandardCharsets.UTF_8).replace('\n',' '));
  c.disconnect();
  if(args.length>0)return;
  try(Socket s=new Socket()){s.connect(new InetSocketAddress(InetAddress.getByName("::ffff:183.2.172.177"),443),5000);System.out.println("MAPPED_TCP=YES");}
  byte[] nonce=("R13UDP-"+System.nanoTime()).getBytes(StandardCharsets.UTF_8);
  try(DatagramSocket d=new DatagramSocket()){
   d.setSoTimeout(5000);d.send(new DatagramPacket(nonce,nonce.length,InetAddress.getByName("192.0.2.10"),19863));
   byte[] r=new byte[128];DatagramPacket p=new DatagramPacket(r,r.length);d.receive(p);
   if(!java.util.Arrays.equals(nonce,java.util.Arrays.copyOf(r,p.getLength())))throw new IOException("UDP mismatch");
   System.out.println("UDP_ECHO=YES bytes="+p.getLength());
  }
 }
}
