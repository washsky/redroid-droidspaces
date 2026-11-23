import java.io.*;
public class RootProbe {
 public static void main(String[] args) throws Exception {
  System.out.println("CALLER_UID="+android.os.Process.myUid());
  Process p=new ProcessBuilder("/system/bin/su","-c","id").redirectErrorStream(true).start();
  StringBuilder out=new StringBuilder();
  try(BufferedReader r=new BufferedReader(new InputStreamReader(p.getInputStream()))){
   String s;while((s=r.readLine())!=null){out.append(s).append('\n');}
  }
  int code=p.waitFor();System.out.println("SU_EXIT="+code);System.out.print(out);
  boolean root=code==0&&out.toString().contains("uid=0(root)");
  System.out.println("ROOT_GRANTED="+root);
  if(args.length>0&&args[0].equals("allow")&&!root)throw new IllegalStateException("Expected root");
  if(args.length>0&&args[0].equals("deny")&&root)throw new IllegalStateException("Unexpected root");
 }
}
