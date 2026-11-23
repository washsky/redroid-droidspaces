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
    TextView statusTitle,statusText,countText,footText;Switch master;
    Button apply,refresh,restart,rootButton;EditText search;CheckBox system,only;
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
        LinearLayout page=vertical();page.setBackgroundColor(BG);page.setFocusableInTouchMode(true);setContentView(page);
        LinearLayout header=vertical();header.setPadding(d(20),d(14),d(20),d(12));page.addView(header,lp(-1,-2));
        TextView eyebrow=text("DROIDSPACES  /  REDROID 13",10,MUTED,true);eyebrow.setLetterSpacing(.12f);header.addView(eyebrow);
        LinearLayout titleRow=new LinearLayout(this);titleRow.setGravity(Gravity.CENTER_VERTICAL);header.addView(titleRow,lp(-1,-2));
        TextView title=text("应用网络代理",25,INK,true);titleRow.addView(title,new LinearLayout.LayoutParams(0,d(48),1));title.setGravity(Gravity.CENTER_VERTICAL);
        Button help=button("说明");titleRow.addView(help,lp(64,40));help.setOnClickListener(v->help());

        LinearLayout card=vertical();card.setPadding(d(16),d(10),d(16),d(10));card.setBackground(shape(Color.WHITE,18));
        LinearLayout.LayoutParams cp=lp(-1,-2);cp.setMargins(d(16),0,d(16),d(12));page.addView(card,cp);
        LinearLayout service=new LinearLayout(this);service.setGravity(Gravity.CENTER_VERTICAL);card.addView(service,lp(-1,46));
        statusTitle=text("正在连接服务…",17,INK,true);service.addView(statusTitle,new LinearLayout.LayoutParams(0,-2,1));
        master=new Switch(this);master.setContentDescription("代理总开关");service.addView(master,lp(52,-2));
        master.setOnCheckedChangeListener((b,on)->{if(!binding){enabled=on;updateDraft();rows.notifyDataSetChanged();}});
        statusText=text("读取已保存的应用名单",12,MUTED,false);statusText.setLineSpacing(d(3),1);card.addView(statusText,lp(-1,-2));
        LinearLayout actions=new LinearLayout(this);actions.setGravity(Gravity.RIGHT);card.addView(actions,lp(-1,40));
        rootButton=button("打开 Magisk");actions.addView(rootButton,lp(100,40));rootButton.setOnClickListener(v->{Intent i=getPackageManager().getLaunchIntentForPackage("com.topjohnwu.magisk");if(i!=null)startActivity(i);else toast("未找到 Magisk 管理器");});
        refresh=button("刷新");actions.addView(refresh,lp(62,40));refresh.setOnClickListener(v->{if(dirty())confirmReload();else load();});
        restart=button("重启服务");actions.addView(restart,lp(94,40));restart.setOnClickListener(v->{if(dirty()){toast("请先保存修改，或刷新后重试");return;}request("restart",null);});

        search=new EditText(this);search.setSingleLine(true);search.setTextSize(14);search.setHint("搜索应用、包名或 UID");search.setTextColor(INK);search.setHintTextColor(MUTED);search.setPadding(d(14),0,d(14),0);search.setBackground(shape(0xffe9edf5,12));
        LinearLayout.LayoutParams sp=lp(-1,44);sp.setMargins(d(16),0,d(16),d(5));page.addView(search,sp);
        search.addTextChangedListener(new TextWatcher(){public void beforeTextChanged(CharSequence s,int a,int c,int f){}public void onTextChanged(CharSequence s,int start,int before,int count){query=s.toString().toLowerCase(Locale.ROOT);filter();}public void afterTextChanged(Editable e){}});
        search.setImeOptions(EditorInfo.IME_ACTION_SEARCH);search.setOnEditorActionListener((v,action,event)->{hideKeyboard();return true;});
        LinearLayout filters=new LinearLayout(this);filters.setPadding(d(12),0,d(12),0);filters.setGravity(Gravity.CENTER_VERTICAL);page.addView(filters,lp(-1,40));
        system=new CheckBox(this);system.setText("系统应用");system.setTextSize(12);system.setTextColor(MUTED);filters.addView(system,lp(-2,-2));system.setOnCheckedChangeListener((x,b)->filter());
        only=new CheckBox(this);only.setText("仅已选择");only.setTextSize(12);only.setTextColor(MUTED);filters.addView(only,lp(-2,-2));only.setOnCheckedChangeListener((x,b)->filter());
        countText=text("",11,MUTED,false);countText.setGravity(Gravity.RIGHT);filters.addView(countText,new LinearLayout.LayoutParams(0,-2,1));
        list=new ListView(this);list.setDivider(null);list.setDividerHeight(0);list.setPadding(d(16),0,d(16),d(8));list.setClipToPadding(false);list.setCacheColorHint(Color.TRANSPARENT);rows=new Rows();list.setAdapter(rows);page.addView(list,new LinearLayout.LayoutParams(-1,0,1));

        LinearLayout bottom=vertical();bottom.setPadding(d(16),d(9),d(16),d(12));bottom.setBackgroundColor(Color.WHITE);page.addView(bottom,lp(-1,-2));
        footText=text("按包名保存，重启容器后自动恢复",11,MUTED,false);bottom.addView(footText,lp(-1,-2));space(bottom,7);
        apply=button("保存并应用");apply.setTextColor(Color.WHITE);apply.setTextSize(15);apply.setTypeface(Typeface.DEFAULT,Typeface.BOLD);apply.setBackground(shape(BLUE,12));bottom.addView(apply,lp(-1,46));apply.setOnClickListener(v->save());
        page.getViewTreeObserver().addOnGlobalLayoutListener(()->{
            android.graphics.Rect frame=new android.graphics.Rect();page.getWindowVisibleDisplayFrame(frame);
            boolean typing=page.getRootView().getHeight()-frame.bottom>d(150);
            int visibility=typing?View.GONE:View.VISIBLE;
            if(header.getVisibility()!=visibility){header.setVisibility(visibility);statusText.setVisibility(visibility);actions.setVisibility(visibility);bottom.setVisibility(visibility);}
        });
        binding=true;master.setChecked(true);binding=false;setBusy(true);
    }
    boolean isSelected(AppGroup g){for(String p:g.packages)if(selected.contains(p))return true;return false;}
    int selectedCount(){int n=0;for(AppGroup g:groups)if(isSelected(g))n++;return n;}
    boolean dirty(){return loaded&&(enabled!=savedEnabled||!selected.equals(saved));}
    void setBusy(boolean b){busy=b;refresh.setEnabled(!b);restart.setEnabled(!b&&root&&snapshot!=null&&snapshot.optBoolean("enabled"));master.setEnabled(!b&&root);apply.setEnabled(!b&&root&&dirty());apply.setAlpha(apply.isEnabled()?1f:.45f);rows.notifyDataSetChanged();}
    void updateDraft(){
        if(footText==null)return;
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
                ArrayList<AppGroup> apps=readApps();JSONObject state=call("status",null);
                main.post(()->{groups.clear();groups.addAll(apps);accept(state);setBusy(false);filter();});
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
        statusTitle.setText(synced?"● 服务运行中":enabled?"● 服务未就绪":"● 代理已关闭");statusTitle.setTextColor(synced?GREEN:enabled?0xffb45309:MUTED);
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
        if(!Arrays.asList("status","save","restart").contains(operation))throw new IOException("BAD_OPERATION");
        java.lang.Process p=new ProcessBuilder("/system/bin/su","-c","/vendor/bin/arm13-netctl "+operation).redirectErrorStream(true).start();
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
    void confirmReload(){new AlertDialog.Builder(this).setTitle("放弃未保存的修改？").setMessage("刷新会重新读取当前已保存的名单。").setNegativeButton("继续编辑",null).setPositiveButton("刷新",(d,w)->load()).show();}
    void help(){new AlertDialog.Builder(this).setTitle("服务如何运行").setMessage("Android init 在容器启动时自动恢复服务。名单按包名保存，服务监听安装和卸载变化，自动更新 UID。\n\n启用的应用共用 shell UID 2000 的网络身份，TLS 仍由原应用处理。这是兼容方案，不是每应用独立网络 UID 隔离。\n\n保存后影响新建 TCP/UDP 连接；已有连接不会被强制断开。关闭代理恢复原生连接，不等于断网。\n\n共享 UID 的应用一起生效；界面按 UID 分组。管理界面需要 Root，网络服务自身以 shell 身份运行。").setPositiveButton("知道了",null).show();}
    void toast(String s){Toast.makeText(this,s,Toast.LENGTH_LONG).show();}
    void hideKeyboard(){((InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).hideSoftInputFromWindow(search.getWindowToken(),0);search.clearFocus();}
    @Override public void onBackPressed(){if(search.hasFocus()){hideKeyboard();return;}if(dirty())new AlertDialog.Builder(this).setTitle("还有未保存修改").setNegativeButton("继续编辑",null).setPositiveButton("退出",(d,w)->finish()).show();else super.onBackPressed();}

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
            Switch toggle=new Switch(MainActivity.this);toggle.setContentDescription("代理："+g.label);toggle.setChecked(on);toggle.setEnabled(root&&!busy);row.addView(toggle,lp(48,-2));
            toggle.setOnCheckedChangeListener((v,checked)->{if(checked)selected.addAll(g.packages);else selected.removeAll(g.packages);updateDraft();main.post(()->filter());});
            row.setOnClickListener(v->{if(toggle.isEnabled())toggle.setChecked(!toggle.isChecked());});
            row.setOnLongClickListener(v->{new AlertDialog.Builder(MainActivity.this).setTitle(g.label).setMessage("UID "+g.uid+"\n\n"+TextUtils.join("\n",g.packages)).setPositiveButton("关闭",null).show();return true;});return wrap;
        }
    }
}
