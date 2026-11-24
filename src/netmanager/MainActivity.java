package lab.arm13.netmanager;

import android.app.*;
import android.os.*;
import android.content.*;
import android.content.pm.*;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.*;
import android.text.*;
import android.view.*;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.EditorInfo;
import android.widget.*;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.text.Collator;
import java.util.*;
import java.util.concurrent.*;

public final class MainActivity extends Activity {
    final int INK=0xff172033,MUTED=0xff68758b,BLUE=0xff2563eb,GREEN=0xff087f68,BG=0xfff4f6fa;
    final Handler main=new Handler(Looper.getMainLooper());
    final ExecutorService worker=Executors.newSingleThreadExecutor();
    final ArrayList<AppGroup> groups=new ArrayList<>(),visible=new ArrayList<>();
    final Set<String> selected=new TreeSet<>(),saved=new TreeSet<>();
    TextView statusTitle,statusText,countText,footText,proxySummary;Switch master;
    LinearLayout homePage,appsPage;ScrollView homeScroll;Button displayButton,appsButton,homeSave;
    Button apply,refresh,restart,rootButton,proxyButton;EditText search;CheckBox system,only;
    ListView list;Rows rows;boolean enabled=true,savedEnabled=true,loaded=false,busy=false,root=false,binding=false;
    long revision;JSONObject snapshot;String query="";

    static final class AppGroup { int uid;String label;Drawable icon;boolean isSystem=true;final ArrayList<String> packages=new ArrayList<>(); }
    int d(float n){return Math.round(n*getResources().getDisplayMetrics().density);}
    TextView text(String s,int size,int color,boolean bold){TextView t=new TextView(this);t.setText(s);t.setTextSize(size);t.setTextColor(color);if(bold)t.setTypeface(Typeface.DEFAULT,Typeface.BOLD);return t;}
    GradientDrawable shape(int color,int radius){GradientDrawable g=new GradientDrawable();g.setColor(color);g.setCornerRadius(d(radius));return g;}
    LinearLayout vertical(){LinearLayout l=new LinearLayout(this);l.setOrientation(LinearLayout.VERTICAL);return l;}
    Button button(String s){Button b=new Button(this);b.setText(s);b.setTextSize(13);b.setAllCaps(false);b.setTextColor(BLUE);b.setMinHeight(d(40));b.setStateListAnimator(null);b.setElevation(0);b.setBackground(new RippleDrawable(android.content.res.ColorStateList.valueOf(0x222563eb),shape(0xffedf3ff,10),null));return b;}
    LinearLayout.LayoutParams lp(int w,int h){return new LinearLayout.LayoutParams(w<0?w:d(w),h<0?h:d(h));}
    void space(LinearLayout l,int height){View v=new View(this);l.addView(v,lp(1,height));}
    @Override public void onCreate(Bundle state){super.onCreate(state);build();load();}
    @Override public void onResume(){super.onResume();if(loaded&&!busy&&!dirty())load();}
    @Override public void onDestroy(){worker.shutdownNow();super.onDestroy();}

    void build(){
        getWindow().setStatusBarColor(BG);getWindow().setNavigationBarColor(Color.WHITE);
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR|View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
        LinearLayout rootPage=vertical();rootPage.setBackgroundColor(BG);setContentView(rootPage);
        homeScroll=new ScrollView(this);homeScroll.setFillViewport(true);rootPage.addView(homeScroll,new LinearLayout.LayoutParams(-1,-1));
        homePage=vertical();homePage.setPadding(d(18),d(18),d(18),d(24));homeScroll.addView(homePage);
        TextView eyebrow=text("DROIDSPACES  /  REDROID 13",10,MUTED,true);eyebrow.setLetterSpacing(.12f);homePage.addView(eyebrow);
        homePage.addView(text("UID 网络管理",25,INK,true));space(homePage,18);
        LinearLayout card=vertical();card.setPadding(d(16),d(16),d(16),d(16));card.setBackground(shape(Color.WHITE,18));homePage.addView(card,lp(-1,-2));
        statusTitle=text("正在连接服务…",17,INK,true);card.addView(statusTitle);space(card,8);
        statusText=text("读取已保存的应用名单",12,MUTED,false);statusText.setLineSpacing(d(3),1);card.addView(statusText);
        proxySummary=text("统一代理：读取中…",12,MUTED,false);space(card,10);card.addView(proxySummary);
        rootButton=button("打开 Magisk");card.addView(rootButton,lp(-1,42));rootButton.setOnClickListener(v->{Intent i=getPackageManager().getLaunchIntentForPackage("com.topjohnwu.magisk");if(i!=null)startActivity(i);else toast("请打开 Magisk 授予 Root");});
        space(homePage,16);
        appsButton=button("应用名单与联网兼容  ›");homePage.addView(appsButton,lp(-1,58));appsButton.setOnClickListener(v->showApps(true));space(homePage,10);
        proxyButton=button("统一代理出口  ›");homePage.addView(proxyButton,lp(-1,58));proxyButton.setOnClickListener(v->openProxy());space(homePage,10);
        displayButton=button("分辨率、DPI 与刷新率  ›");homePage.addView(displayButton,lp(-1,58));displayButton.setOnClickListener(v->openDisplay());space(homePage,18);
        LinearLayout actions=new LinearLayout(this);homePage.addView(actions,lp(-1,44));
        refresh=button("刷新");actions.addView(refresh,new LinearLayout.LayoutParams(0,-1,1));refresh.setOnClickListener(v->{if(dirty())confirmReload();else load();});
        restart=button("重启联网服务");actions.addView(restart,new LinearLayout.LayoutParams(0,-1,1));restart.setOnClickListener(v->request("restart",null));
        Button help=button("说明");space(homePage,10);homePage.addView(help,lp(-1,44));help.setOnClickListener(v->help());
        homeSave=button("保存应用名单修改");space(homePage,10);homePage.addView(homeSave,lp(-1,46));homeSave.setOnClickListener(v->save());

        appsPage=vertical();appsPage.setVisibility(View.GONE);rootPage.addView(appsPage,new LinearLayout.LayoutParams(-1,-1));
        LinearLayout bar=new LinearLayout(this);bar.setPadding(d(12),d(8),d(12),d(4));bar.setGravity(Gravity.CENTER_VERTICAL);appsPage.addView(bar,lp(-1,54));
        Button back=button("返回");bar.addView(back,lp(64,40));back.setOnClickListener(v->{hideKeyboard();showApps(false);});
        TextView title=text("应用名单",20,INK,true);title.setGravity(Gravity.CENTER_VERTICAL);bar.addView(title,new LinearLayout.LayoutParams(0,-1,1));
        master=new Switch(this);master.setContentDescription("联网兼容总开关");bar.addView(master,lp(54,-2));
        master.setOnCheckedChangeListener((v,on)->{if(!binding){enabled=on;updateDraft();rows.notifyDataSetChanged();}});
        search=new EditText(this);search.setSingleLine(true);search.setTextSize(14);search.setHint("搜索名称、包名或 UID");search.setContentDescription("搜索应用");search.setImeOptions(EditorInfo.IME_ACTION_DONE);
        LinearLayout.LayoutParams sp=lp(-1,48);sp.setMargins(d(16),d(4),d(16),0);appsPage.addView(search,sp);
        search.addTextChangedListener(new TextWatcher(){public void beforeTextChanged(CharSequence x,int st,int c,int after){}public void onTextChanged(CharSequence x,int st,int before,int count){query=x.toString().trim().toLowerCase(Locale.ROOT);filter();}public void afterTextChanged(Editable x){}});
        search.setOnEditorActionListener((v,action,event)->{hideKeyboard();return true;});
        LinearLayout filters=new LinearLayout(this);filters.setPadding(d(12),0,d(12),0);filters.setGravity(Gravity.CENTER_VERTICAL);appsPage.addView(filters,lp(-1,40));
        system=new CheckBox(this);system.setText("系统应用");system.setTextSize(12);filters.addView(system,lp(-2,-2));system.setOnCheckedChangeListener((v,on)->filter());
        only=new CheckBox(this);only.setText("仅已选择");only.setTextSize(12);filters.addView(only,lp(-2,-2));only.setOnCheckedChangeListener((v,on)->filter());
        countText=text("",11,MUTED,false);countText.setGravity(Gravity.RIGHT);filters.addView(countText,new LinearLayout.LayoutParams(0,-2,1));
        list=new ListView(this);list.setDivider(null);list.setDividerHeight(0);list.setPadding(d(16),0,d(16),d(8));list.setClipToPadding(false);rows=new Rows();list.setAdapter(rows);appsPage.addView(list,new LinearLayout.LayoutParams(-1,0,1));
        LinearLayout bottom=vertical();bottom.setPadding(d(16),d(8),d(16),d(10));bottom.setBackgroundColor(Color.WHITE);appsPage.addView(bottom,lp(-1,-2));
        footText=text("",11,MUTED,false);bottom.addView(footText);space(bottom,6);
        apply=button("保存并应用");apply.setTextColor(Color.WHITE);apply.setBackground(shape(BLUE,12));bottom.addView(apply,lp(-1,46));apply.setOnClickListener(v->save());
        binding=true;master.setChecked(true);binding=false;setBusy(true);
    }
    void showApps(boolean show){homeScroll.setVisibility(show?View.GONE:View.VISIBLE);appsPage.setVisibility(show?View.VISIBLE:View.GONE);}
    boolean isSelected(AppGroup g){for(String p:g.packages)if(selected.contains(p))return true;return false;}
    int selectedCount(){int n=0;for(AppGroup g:groups)if(isSelected(g))n++;return n;}
    boolean dirty(){return loaded&&(enabled!=savedEnabled||!selected.equals(saved));}
    void setBusy(boolean b){busy=b;displayButton.setEnabled(!b&&root);appsButton.setEnabled(!b&&root);homeSave.setEnabled(!b&&root&&dirty());refresh.setEnabled(!b);proxyButton.setEnabled(!b&&root);restart.setEnabled(!b&&root&&snapshot!=null&&snapshot.optBoolean("enabled"));master.setEnabled(!b&&root);apply.setEnabled(!b&&root&&dirty());apply.setAlpha(apply.isEnabled()?1f:.45f);rows.notifyDataSetChanged();}
    void updateDraft(){
        if(footText==null)return;
        homeSave.setVisibility(dirty()?View.VISIBLE:View.GONE);homeSave.setEnabled(root&&!busy&&dirty());
        footText.setText(dirty()?"有未保存修改 · 仅影响新建连接":"按包名保存 · 容器重启及应用重装会自动同步");
        apply.setText(dirty()?"保存并应用（"+selectedCount()+" 个 UID）":"设置已保存");apply.setEnabled(root&&!busy&&dirty());apply.setAlpha(apply.isEnabled()?1f:.45f);
    }
    void filter(){
        visible.clear();if(system==null)return;
        for(AppGroup g:groups){
            if(!system.isChecked()&&g.isSystem&&!isSelected(g))continue;
            if(only.isChecked()&&!isSelected(g))continue;
            String hay=(g.label+" "+g.uid+" "+g.packages.toString()).toLowerCase(Locale.ROOT);
            if(!query.isEmpty()&&!hay.contains(query))continue;visible.add(g);
        }
        Collections.sort(visible,(a,b)->{boolean x=isSelected(a),y=isSelected(b);if(x!=y)return x?-1:1;return Collator.getInstance(Locale.CHINA).compare(a.label,b.label);});
        countText.setText(visible.size()+" 项");rows.notifyDataSetChanged();updateDraft();
    }
    void load(){
        if(busy&&loaded)return;setBusy(true);
        worker.execute(()->{
            try{
                ArrayList<AppGroup> apps=readApps();JSONObject state=call("status",null);JSONObject proxy;
                try{proxy=call("proxy-status",null);}catch(Exception ignored){proxy=null;}
                final JSONObject proxyResult=proxy;
                main.post(()->{groups.clear();groups.addAll(apps);accept(state);if(proxyResult!=null)proxyState(proxyResult);else proxySummary.setText("统一代理：管理接口未就绪");setBusy(false);filter();});
            }catch(Exception e){main.post(()->showError(e));}
        });
    }
    ArrayList<AppGroup> readApps(){
        PackageManager pm=getPackageManager();HashMap<Integer,AppGroup> map=new HashMap<>();
        for(ApplicationInfo ai:pm.getInstalledApplications(0)){
            if(ai.uid<10000||pm.checkPermission("android.permission.INTERNET",ai.packageName)!=PackageManager.PERMISSION_GRANTED)continue;
            AppGroup g=map.get(ai.uid);if(g==null){g=new AppGroup();g.uid=ai.uid;g.label=pm.getApplicationLabel(ai).toString();g.icon=pm.getApplicationIcon(ai);map.put(ai.uid,g);}
            g.packages.add(ai.packageName);g.isSystem&=(ai.flags&ApplicationInfo.FLAG_SYSTEM)!=0;
        }return new ArrayList<>(map.values());
    }
    void accept(JSONObject s){
        snapshot=s;root=true;loaded=true;revision=s.optLong("revision");enabled=s.optBoolean("enabled");savedEnabled=enabled;
        selected.clear();JSONArray a=s.optJSONArray("packages");if(a!=null)for(int i=0;i<a.length();i++)selected.add(a.optString(i));saved.clear();saved.addAll(selected);
        Set<String> known=new HashSet<>();for(AppGroup g:groups)known.addAll(g.packages);
        for(String p:selected)if(!known.contains(p)){AppGroup g=new AppGroup();g.uid=-1;g.label=p;g.packages.add(p);g.isSystem=false;groups.add(g);}
        String st=s.optString("service");boolean running="running".equals(st);boolean synced=running&&enabled&&s.optLong("activeRevision")==revision;
        statusTitle.setText(synced?"● 服务运行中":enabled?"● 服务未就绪":"● 联网兼容已关闭");statusTitle.setTextColor(synced?GREEN:enabled?0xffb45309:MUTED);
        int count=s.optJSONArray("uids")==null?0:s.optJSONArray("uids").length();
        statusText.setText((enabled?"当前覆盖 "+count+" 个 UID":"已保留应用名单")+" · 网络身份 UID 2000\n配置保存在容器 /data，重启后自动恢复");
        binding=true;master.setChecked(enabled);binding=false;rootButton.setVisibility(View.GONE);updateDraft();
    }
    void showError(Exception e){
        root=false;loaded=true;setBusy(false);statusTitle.setText("● 无法连接管理接口");statusTitle.setTextColor(0xffb45309);
        String message=e.getMessage()==null?"操作失败":e.getMessage();
        if(message.contains("Permission denied")||message.contains("ROOT_REQUIRED"))message="需要 Root：在 Magisk → 超级用户 → ＋ 中选择本应用授权，然后刷新。";
        statusText.setText(message);rootButton.setVisibility(View.VISIBLE);footText.setText("未修改现有配置");apply.setEnabled(false);apply.setAlpha(.45f);
    }
    void save(){
        if(!dirty())return;StringBuilder input=new StringBuilder().append(revision).append('\n').append(enabled?"1\n":"0\n");
        for(String p:selected)input.append(p).append('\n');request("save",input.toString());
    }
    void request(String operation,String input){
        setBusy(true);worker.execute(()->{
            try{JSONObject state=call(operation,input);main.post(()->{
                accept(state);setBusy(false);filter();String st=state.optString("service");
                boolean ready=state.optBoolean("enabled")?"running".equals(st)&&state.optLong("activeRevision")==state.optLong("revision"):"stopped".equals(st);
                toast(ready?(operation.equals("restart")?"服务已重启":"已保存并应用"):"设置已保存，请刷新检查服务状态");
            });}catch(Exception e){main.post(()->{setBusy(false);toast(errorLabel(e.getMessage()));statusText.setText(errorLabel(e.getMessage()));});}
        });
    }
    String errorLabel(String s){if(s==null)return "操作失败";if(s.contains("STALE_CONFIG"))return "配置已变化，请刷新后重试";if(s.contains("BAD_PACKAGE"))return "应用包名无效，未修改配置";return s;}
    JSONObject call(String operation,String input)throws Exception{
        if(!Arrays.asList("status","save","restart","proxy-status","proxy-save","proxy-test","boot-status","boot-save").contains(operation))throw new IOException("BAD_OPERATION");
        // The app inherits the Zygote socket broker preload. A vendor control
        // executable cannot load that system library in its linker namespace.
        String command=operation.startsWith("proxy-")?"/vendor/bin/arm13-proxyctl "+operation.substring(6):operation.startsWith("boot-")?"/vendor/bin/arm13-bootctl "+operation.substring(5):"/vendor/bin/arm13-netctl "+operation;
        ProcessBuilder builder=new ProcessBuilder("/system/bin/su","-c",command);
        builder.environment().remove("LD_PRELOAD");
        java.lang.Process p=builder.redirectErrorStream(true).start();
        ScheduledExecutorService timer=Executors.newSingleThreadScheduledExecutor();timer.schedule(()->p.destroyForcibly(),20,TimeUnit.SECONDS);
        try{
            try(OutputStream o=p.getOutputStream()){if(input!=null)o.write(input.getBytes(StandardCharsets.UTF_8));}
            ByteArrayOutputStream bytes=new ByteArrayOutputStream();try(InputStream in=p.getInputStream()){
                byte[] buf=new byte[4096];int n;while((n=in.read(buf))!=-1){if(bytes.size()+n>262144)throw new IOException("RESPONSE_TOO_LARGE");bytes.write(buf,0,n);}
            }
            int code=p.waitFor();String out=bytes.toString("UTF-8").trim();int start=out.indexOf('{');
            if(start<0)throw new IOException(out.isEmpty()?"管理接口没有响应，请检查 Root 权限":out);
            JSONObject result=new JSONObject(out.substring(start));if(code!=0||!result.optBoolean("ok"))throw new IOException(result.optString("error",out));return result;
        }finally{timer.shutdownNow();p.destroy();}
    }
    void proxyState(JSONObject s){
        boolean on=s.optBoolean("enabled"),active=s.optBoolean("active");
        proxySummary.setText(on?"统一代理："+(s.optString("protocol").equals("http")?"HTTP":"SOCKS5")+" "+s.optString("host")+":"+s.optInt("port")+"\n"+(active?"已启用 · 转发计数 "+s.optInt("connections")+" 次"+(s.optBoolean("udp")?" · UDP 开":"")+(s.optBoolean("dns")?" · DNS 开":""):"服务未就绪"):"统一代理：关闭");
        proxySummary.setTextColor(on&&!active?0xffb45309:MUTED);
    }
    void openProxy(){
        if(busy||!root)return;setBusy(true);worker.execute(()->{
            try{JSONObject s=call("proxy-status",null);main.post(()->{setBusy(false);proxyState(s);proxyDialog(s);});}
            catch(Exception e){main.post(()->{setBusy(false);toast(e.getMessage());});}
        });
    }
    EditText field(LinearLayout form,String label,String value,int type){
        form.addView(text(label,12,MUTED,true));EditText edit=new EditText(this);edit.setSingleLine(true);edit.setContentDescription(label);edit.setText(value);edit.setInputType(type);form.addView(edit,lp(-1,48));return edit;
    }
    String hex(String value){byte[] bytes=value.getBytes(StandardCharsets.UTF_8);StringBuilder out=new StringBuilder();for(byte x:bytes)out.append(String.format(Locale.ROOT,"%02x",x&255));return out.length()==0?"-":out.toString();}
    String unhex(String value){try{if(value.equals("-"))return "";byte[] bytes=new byte[value.length()/2];for(int i=0;i<bytes.length;i++)bytes[i]=(byte)Integer.parseInt(value.substring(i*2,i*2+2),16);return new String(bytes,StandardCharsets.UTF_8);}catch(Exception e){return "";}}
    void proxyDialog(JSONObject savedProxy){
        LinearLayout form=vertical();form.setPadding(d(22),d(8),d(22),d(16));
        Switch on=new Switch(this);on.setText("启用统一代理");on.setContentDescription("统一代理开关");on.setChecked(savedProxy.optBoolean("enabled"));form.addView(on,lp(-1,48));
        Spinner protocol=new Spinner(this);protocol.setContentDescription("代理协议");protocol.setAdapter(new ArrayAdapter<String>(this,android.R.layout.simple_spinner_dropdown_item,new String[]{"SOCKS5","HTTP CONNECT"}));protocol.setSelection(savedProxy.optString("protocol").equals("http")?1:0);form.addView(protocol,lp(-1,44));
        EditText host=field(form,"代理服务器",savedProxy.optString("host","192.168.2.95"),android.text.InputType.TYPE_CLASS_PHONE);
        EditText port=field(form,"代理端口",String.valueOf(savedProxy.optInt("port",10808)),android.text.InputType.TYPE_CLASS_NUMBER);
        Switch auth=new Switch(this);auth.setText("使用用户名与密码认证");auth.setContentDescription("代理认证开关");auth.setChecked(savedProxy.optBoolean("auth"));form.addView(auth,lp(-1,48));
        EditText user=field(form,"代理用户名",unhex(savedProxy.optString("usernameHex","-")),android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        EditText password=field(form,"代理密码","",android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_VARIATION_PASSWORD);
        password.setHint(savedProxy.optBoolean("hasPassword")?"留空保留已保存密码":"输入代理密码");
        password.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO);
        Switch udp=new Switch(this);udp.setText("代理公网 UDP（仅 SOCKS5）");udp.setContentDescription("UDP 代理开关");udp.setChecked(savedProxy.optBoolean("udp"));form.addView(udp,lp(-1,48));
        Switch dns=new Switch(this);dns.setText("DNS 经代理（DNS over TCP）");dns.setContentDescription("DNS 代理开关");dns.setChecked(savedProxy.optBoolean("dns"));form.addView(dns,lp(-1,48));
        EditText dnsHost=field(form,"上游 DNS IPv4",savedProxy.optString("dnsHost","1.1.1.1"),android.text.InputType.TYPE_CLASS_PHONE);
        Runnable controls=()->{boolean a=auth.isChecked();user.setEnabled(a);password.setEnabled(a);boolean socks=protocol.getSelectedItemPosition()==0;udp.setEnabled(socks);if(!socks)udp.setChecked(false);dnsHost.setEnabled(dns.isChecked());};
        auth.setOnCheckedChangeListener((v,b)->controls.run());dns.setOnCheckedChangeListener((v,b)->controls.run());
        protocol.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){public void onItemSelected(AdapterView<?> p,View v,int pos,long id){controls.run();}public void onNothingSelected(AdapterView<?> p){}});controls.run();
        form.addView(text("覆盖本实例 IPv4 公网 TCP；UDP 需服务器支持 UDP ASSOCIATE。局域网及回环通常直连；启用 DNS 时，UDP 53 查询改经代理的 TCP 连接送往所选 DNS:53。IPv6、应用自带 DoH/DoT 不受此 DNS 开关控制。\n\n启用的代理路径故障时不会自动直连。应用名单仍只控制 UID 联网兼容。保存会重连代理连接。",12,MUTED,false));
        ScrollView scroll=new ScrollView(this);scroll.addView(form);
        AlertDialog dialog=new AlertDialog.Builder(this).setTitle("统一代理出口").setView(scroll).setNegativeButton("取消",null).setNeutralButton("测试已保存 TCP",null).setPositiveButton("保存并应用",null).create();
        dialog.setOnShowListener(v->{
            dialog.getButton(AlertDialog.BUTTON_NEUTRAL).setOnClickListener(x->{
                if(busy)return;setBusy(true);worker.execute(()->{try{call("proxy-test",null);main.post(()->{setBusy(false);toast("已保存出口 TCP 握手成功");});}catch(Exception ex){main.post(()->{setBusy(false);toast("代理连接失败："+ex.getMessage());});}});
            });
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(x->{
                if(busy)return;String address=host.getText().toString().trim(),dnsAddress=dnsHost.getText().toString().trim();int number;
                try{number=Integer.parseInt(port.getText().toString().trim());}catch(Exception ex){toast("请输入有效端口");return;}
                if(!address.matches("[0-9]{1,3}(\\.[0-9]{1,3}){3}")||!dnsAddress.matches("[0-9]{1,3}(\\.[0-9]{1,3}){3}")||number<1||number>65535){toast("请检查 IPv4 地址和端口");return;}
                String username=user.getText().toString(),secret=password.getText().toString();
                if(auth.isChecked()&&secret.isEmpty()&&savedProxy.optBoolean("hasPassword"))secret="__KEEP_STORED_PASSWORD__";
                if(auth.isChecked()&&(username.isEmpty()||username.contains(":")||secret.isEmpty()||username.getBytes(StandardCharsets.UTF_8).length>255||secret.getBytes(StandardCharsets.UTF_8).length>255)){toast("请填写认证用户名与密码，最长各 255 字节，用户名不能含冒号");return;}
                String input=savedProxy.optLong("revision")+"\n"+(on.isChecked()?1:0)+"\n"+(protocol.getSelectedItemPosition()==1?"http":"socks5")+"\n"+address+"\n"+number+"\n"+(udp.isChecked()?1:0)+"\n"+(dns.isChecked()?1:0)+"\n"+dnsAddress+"\n"+(auth.isChecked()?1:0)+"\n"+hex(auth.isChecked()?username:"")+"\n"+hex(auth.isChecked()?secret:"")+"\n";
                setBusy(true);worker.execute(()->{try{JSONObject state=call("proxy-save",input);main.post(()->{proxyState(state);setBusy(false);dialog.dismiss();toast("统一代理设置已保存");});}catch(Exception ex){main.post(()->{setBusy(false);toast(errorLabel(ex.getMessage()));});}});
            });
        });dialog.show();
    }
    void openDisplay(){
        if(busy||!root)return;setBusy(true);worker.execute(()->{try{JSONObject state=call("boot-status",null);main.post(()->{setBusy(false);displayDialog(state);});}catch(Exception e){main.post(()->{setBusy(false);toast(e.getMessage());});}});
    }
    void displayDialog(JSONObject state){
        LinearLayout form=vertical();form.setPadding(d(22),d(8),d(22),d(16));
        form.addView(text("当前启动："+state.optInt("currentWidth")+" × "+state.optInt("currentHeight")+" · DPI "+state.optInt("currentDpi")+" · "+state.optInt("currentFps")+" Hz",14,INK,true));
        String[] names={"宽度（240–3840，偶数）","高度（240–3840，偶数）","DPI（72–640）","刷新率 Hz（15–120）"},keys={"width","height","dpi","fps"};EditText[] edits=new EditText[4];
        for(int i=0;i<4;i++)edits[i]=field(form,names[i],String.valueOf(state.optInt(keys[i])),android.text.InputType.TYPE_CLASS_NUMBER);
        form.addView(text("保存到 /data/adb/arm13-boot.conf，经 init 校验后供当前容器下次冷启动读取。当前画面不会立即变化；不重启真机。\n"+(state.optBoolean("pending")?"已有待冷启动生效的参数。":"已保存参数与当前启动一致。"),12,MUTED,false));
        ScrollView scroll=new ScrollView(this);scroll.addView(form);
        AlertDialog dialog=new AlertDialog.Builder(this).setTitle("显示启动参数").setView(scroll).setNegativeButton("取消",null).setPositiveButton("保存下次启动参数",null).create();
        dialog.setOnShowListener(v->dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(x->{
            if(busy)return;int[] n=new int[4];try{for(int i=0;i<4;i++)n[i]=Integer.parseInt(edits[i].getText().toString().trim());}catch(Exception ex){toast("请输入整数");return;}
            if(n[0]<240||n[0]>3840||(n[0]&1)!=0||n[1]<240||n[1]>3840||(n[1]&1)!=0||n[2]<72||n[2]>640||n[3]<15||n[3]>120){toast("参数超出范围，宽高必须为偶数");return;}
            String input=state.optLong("revision")+"\n"+n[0]+"\n"+n[1]+"\n"+n[2]+"\n"+n[3]+"\n";
            setBusy(true);worker.execute(()->{try{call("boot-save",input);main.post(()->{setBusy(false);dialog.dismiss();toast("参数已保存，下次冷启动生效");});}catch(Exception ex){main.post(()->{setBusy(false);toast(errorLabel(ex.getMessage()));});}});
        }));dialog.show();
    }
    void confirmReload(){new AlertDialog.Builder(this).setTitle("放弃未保存的修改？").setMessage("刷新会重新读取当前已保存的名单。").setNegativeButton("继续编辑",null).setPositiveButton("刷新",(d,w)->load()).show();}
    void help(){new AlertDialog.Builder(this).setTitle("联网兼容、代理与显示").setMessage("应用名单控制 UID 联网兼容，仍依赖 UID 2000 broker，名单按包名保存。共享 UID 的应用一起生效。\n\n统一代理控制全实例 IPv4 公网 TCP。SOCKS5 可选 UDP；DNS 开关将 UDP 53 查询通过代理 TCP 转发。局域网及回环通常直连，DNS 开启时查询优先转发。IPv6 仍走原路径。认证支持 SOCKS5 用户名密码与 HTTP Basic。\n\n代理转发服务以 shell 身份运行。配置中的认证信息仅 root 与 shell 可读，界面不回显已保存密码。\n\n显示参数保存后在当前容器下次冷启动生效，不会立即重启。").setPositiveButton("知道了",null).show();}
    void toast(String s){Toast.makeText(this,s,Toast.LENGTH_LONG).show();}
    void hideKeyboard(){((InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).hideSoftInputFromWindow(search.getWindowToken(),0);search.clearFocus();}
    @Override public void onBackPressed(){if(search.hasFocus()){hideKeyboard();return;}if(appsPage.getVisibility()==View.VISIBLE){showApps(false);return;}if(dirty())new AlertDialog.Builder(this).setTitle("还有未保存修改").setNegativeButton("继续编辑",null).setPositiveButton("退出",(d,w)->finish()).show();else super.onBackPressed();}

    final class Rows extends BaseAdapter {
        public int getCount(){return visible.size();}public Object getItem(int p){return visible.get(p);}public long getItemId(int p){return p;}
        public View getView(int position,View recycled,android.view.ViewGroup parent){
            AppGroup g=visible.get(position);boolean on=isSelected(g);
            LinearLayout wrap=vertical();wrap.setPadding(0,0,0,d(8));
            LinearLayout row=new LinearLayout(MainActivity.this);row.setGravity(Gravity.CENTER_VERTICAL);row.setPadding(d(12),d(12),d(10),d(12));
            GradientDrawable bg=shape(Color.WHITE,14);if(on)bg.setStroke(d(1),0xffd9e5ff);row.setBackground(bg);wrap.addView(row,lp(-1,82));
            ImageView icon=new ImageView(MainActivity.this);icon.setImageDrawable(g.icon);if(g.icon==null)icon.setImageResource(android.R.drawable.ic_menu_help);row.addView(icon,lp(36,36));
            LinearLayout labels=vertical();LinearLayout.LayoutParams lbl=new LinearLayout.LayoutParams(0,-2,1);lbl.setMargins(d(10),0,d(5),0);row.addView(labels,lbl);
            TextView title=text(g.label,15,INK,true);title.setSingleLine();title.setEllipsize(TextUtils.TruncateAt.END);labels.addView(title);
            TextView pkg=text(g.packages.get(0),11,MUTED,false);pkg.setSingleLine();pkg.setEllipsize(TextUtils.TruncateAt.MIDDLE);labels.addView(pkg);
            String meta=g.uid<0?"未安装或无联网权限 · 保留选择":("UID "+g.uid+(g.packages.size()>1?" · 共享 "+g.packages.size()+" 个包":"")+(on?" · 已选择":""));
            TextView uid=text(meta,10,on?BLUE:MUTED,false);labels.addView(uid);
            Switch toggle=new Switch(MainActivity.this);toggle.setContentDescription("联网兼容："+g.label);toggle.setChecked(on);toggle.setEnabled(root&&!busy);row.addView(toggle,lp(48,-2));
            toggle.setOnCheckedChangeListener((v,checked)->{if(checked)selected.addAll(g.packages);else selected.removeAll(g.packages);updateDraft();main.post(()->filter());});
            row.setOnClickListener(v->{if(toggle.isEnabled())toggle.setChecked(!toggle.isChecked());});
            row.setOnLongClickListener(v->{new AlertDialog.Builder(MainActivity.this).setTitle(g.label).setMessage("UID "+g.uid+"\n\n"+TextUtils.join("\n",g.packages)).setPositiveButton("关闭",null).show();return true;});return wrap;
        }
    }
}
