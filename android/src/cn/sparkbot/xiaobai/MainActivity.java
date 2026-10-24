package cn.sparkbot.xiaobai;

import android.app.*;
import android.os.*;
import android.content.*;
import android.content.res.Configuration;
import android.graphics.*;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.content.res.ColorStateList;
import android.animation.ValueAnimator;
import android.net.*;
import android.net.wifi.WifiManager;
import android.view.*;
import android.webkit.*;
import android.widget.*;
import java.net.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import org.json.*;

/** Local-only controller. No JavaScript bridge, account, cloud or background scan. */
public final class MainActivity extends Activity {
 private final ExecutorService worker=Executors.newSingleThreadExecutor();
 private final LinkedHashMap<String,Device> devices=new LinkedHashMap<>();
 private ConnectivityManager connectivity; private Network wifi;
 private LinearLayout root,cards; private TextView message; private WebView web;
 private volatile String currentHost=""; private volatile boolean scanning=false,alive=true; private volatile int scanGeneration=0; private int connectionGeneration=0;
 private volatile DatagramSocket activeSocket; private int theme=0;
 private final int[][] themes={{0xffedf3ef,0xffe1ede6,0xff243a35,0xff357965},{0xfff1eef8,0xffe5def2,0xff403450,0xff795ca2},{0xfff8f1e7,0xfff0e1ce,0xff51412f,0xff9c663d},{0xffedf3f9,0xffdce8f4,0xff273c53,0xff3a77a9},{0xfff7eef2,0xffefdee6,0xff50394a,0xffa06082},{0xff141c25,0xff273445,0xffeff6ff,0xffadcfed}};
 private static final class Device {String id,name,host;Device(String i,String n,String h){id=i;name=n;host=h;}}
 @Override public void onCreate(Bundle state){super.onCreate(state);connectivity=getSystemService(ConnectivityManager.class);theme=Math.max(0,Math.min(themes.length-1,getPreferences(0).getInt("theme",0)));showHome();discover();}
 private int dp(float x){return (int)(x*getResources().getDisplayMetrics().density+.5f);}
 private GradientDrawable surface(int color,int radius){GradientDrawable g=new GradientDrawable();g.setColor(color);g.setCornerRadius(dp(radius));return g;}
 private TextView label(String text,int size,boolean bold){TextView t=new TextView(this);t.setText(text);t.setTextSize(size);t.setTextColor(themes[theme][2]);if(bold)t.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));t.setPadding(0,dp(6),0,dp(6));return t;}
 private Button button(String text,Runnable action){Button b=new Button(this);b.setText(text);b.setAllCaps(false);b.setTextSize(15);b.setTextColor(themes[theme][2]);b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x18000000|(themes[theme][3]&0xffffff)),surface(theme==5?0xff283646:0xccffffff,16),null));LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,dp(52));p.topMargin=dp(10);b.setLayoutParams(p);b.setOnClickListener(v->action.run());return b;}
 private void createRoot(){root=new LinearLayout(this);root.setOrientation(1);root.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,new int[]{themes[theme][0],themes[theme][1]}));root.setPadding(dp(22),dp(18),dp(22),dp(14));setContentView(root);root.setOnApplyWindowInsetsListener((v,insets)->{int side="controller".equals(root.getTag())?0:dp(22);root.setPadding(side+insets.getSystemWindowInsetLeft(),insets.getSystemWindowInsetTop()+dp(8),side+insets.getSystemWindowInsetRight(),insets.getSystemWindowInsetBottom());return insets;});root.requestApplyInsets();root.setAlpha(0f);root.setTranslationY(dp(8));root.animate().alpha(1f).translationY(0f).setDuration(ValueAnimator.areAnimatorsEnabled()?200:0).setInterpolator(new android.view.animation.DecelerateInterpolator()).start();}
 private void chooseTheme(View anchor){PopupMenu menu=new PopupMenu(this,anchor);String[] names={"森林","星云","日落","晴空","桃雾","夜航"};for(int i=0;i<names.length;i++)menu.getMenu().add(0,i,i,names[i]).setCheckable(true).setChecked(theme==i);menu.setOnMenuItemClickListener(item->{theme=item.getItemId();getPreferences(0).edit().putInt("theme",theme).apply();showHome();return true;});menu.show();}
 private void showHome(){closeWeb();currentHost="";connectivity.bindProcessToNetwork(null);createRoot();
  ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setClipToPadding(false);root.addView(scroll,new LinearLayout.LayoutParams(-1,-1));LinearLayout content=new LinearLayout(this);content.setOrientation(1);scroll.addView(content);
  LinearLayout top=new LinearLayout(this);top.setGravity(Gravity.CENTER_VERTICAL);top.addView(label("小白随行",25,true),new LinearLayout.LayoutParams(0,-2,1));Button palette=button("外观",()->{});palette.setOnClickListener(v->chooseTheme(v));top.addView(palette,new LinearLayout.LayoutParams(dp(72),dp(44)));content.addView(top);
  TextView caption=label("小白，在你身边。",27,true);caption.setPadding(0,dp(25),0,0);content.addView(caption);content.addView(label("轻碰有回应，摸摸更开心。",14,false));
  RobotView robot=new RobotView(this);content.addView(robot,new LinearLayout.LayoutParams(-1,dp(getResources().getConfiguration().orientation==2?130:205)));
  LinearLayout nearby=new LinearLayout(this);nearby.setGravity(Gravity.CENTER_VERTICAL);nearby.addView(label("我的小白",21,true),new LinearLayout.LayoutParams(0,-2,1));Button refresh=button("刷新",this::discover);nearby.addView(refresh,new LinearLayout.LayoutParams(dp(72),dp(44)));content.addView(nearby);
  message=label(scanning?"正在寻找同一 Wi-Fi 中的小白…":"连接同一 Wi-Fi，选择小白即可出发。",13,false);content.addView(message);cards=new LinearLayout(this);cards.setOrientation(1);content.addView(cards);renderDevices();
  LinearLayout ways=new LinearLayout(this);for(int i=0;i<2;i++){Button b=i==0?button("输入 IP",this::manual):button("Wi-Fi 设置",()->startActivity(new Intent(android.provider.Settings.ACTION_WIFI_SETTINGS)));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,dp(48),1);lp.setMargins(i==0?0:dp(6),dp(18),i==0?dp(6):0,0);ways.addView(b,lp);}content.addView(ways);
  TextView note=label("名字与造型保存在小白上。\n找不到设备时，可连接 SparkBot 热点后输入 192.168.4.1。",12,false);note.setPadding(0,dp(20),0,dp(24));content.addView(note);
 }
 private void renderDevices(){if(cards==null)return;cards.removeAllViews();for(Device d:devices.values()){
  LinearLayout card=new LinearLayout(this);card.setGravity(Gravity.CENTER_VERTICAL);card.setPadding(dp(18),dp(15),dp(14),dp(15));card.setBackground(surface(theme==5?0xff202c39:0xeefeffff,22));LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,-2);p.topMargin=dp(12);cards.addView(card,p);
  LinearLayout info=new LinearLayout(this);info.setOrientation(1);info.addView(label(d.name,21,true));info.addView(label(d.host,12,false));card.addView(info,new LinearLayout.LayoutParams(0,-2,1));Button connect=button("连接  ›",()->verifyAndOpen(d.host,d.id));connect.setContentDescription("连接"+d.name);card.addView(connect,new LinearLayout.LayoutParams(dp(90),dp(48)));
 }}
 private Network findWifi(){for(Network n:connectivity.getAllNetworks()){NetworkCapabilities c=connectivity.getNetworkCapabilities(n);if(c!=null&&c.hasTransport(NetworkCapabilities.TRANSPORT_WIFI))return n;}return null;}
 private static boolean privateIp(String host){String[] a=host.split("\\.",-1);if(a.length!=4)return false;int[] b=new int[4];try{for(int i=0;i<4;i++){if(!a[i].matches("[0-9]{1,3}"))return false;b[i]=Integer.parseInt(a[i]);if(b[i]>255)return false;}}catch(Exception e){return false;}return b[0]==10||(b[0]==192&&b[1]==168)||(b[0]==172&&b[1]>=16&&b[1]<=31);}
 private void discover(){if(scanning)return;wifi=findWifi();if(wifi==null){message.setText("还没有连接 Wi-Fi，请先打开 Wi-Fi 设置。");return;}scanning=true;final int generation=++scanGeneration;devices.clear();renderDevices();message.setText("正在寻找附近的小白…");final Network network=wifi;
  worker.execute(()->{String failure=null;WifiManager.MulticastLock lock=null;try{
    WifiManager manager=(WifiManager)getApplicationContext().getSystemService(WIFI_SERVICE);lock=manager.createMulticastLock("xiaobai-discovery");lock.setReferenceCounted(false);lock.acquire();
    Set<String> targets=new LinkedHashSet<>();targets.add("255.255.255.255");LinkProperties props=connectivity.getLinkProperties(network);
    if(props!=null)for(LinkAddress link:props.getLinkAddresses())if(link.getAddress() instanceof Inet4Address){byte[] bytes=link.getAddress().getAddress();int ip=0;for(byte b:bytes)ip=(ip<<8)|(b&255);int bits=link.getPrefixLength();if(bits>0&&bits<31){int broadcast=ip|(-1>>>bits);targets.add(((broadcast>>>24)&255)+"."+((broadcast>>>16)&255)+"."+((broadcast>>>8)&255)+"."+(broadcast&255));}}
    try(DatagramSocket socket=new DatagramSocket(null)){activeSocket=socket;network.bindSocket(socket);socket.setReuseAddress(true);socket.bind(new InetSocketAddress(0));socket.setBroadcast(true);socket.setSoTimeout(220);byte[] probe="XIAOBAI_DISCOVER_V1".getBytes(StandardCharsets.US_ASCII);long end=SystemClock.elapsedRealtime()+4500,next=0;
      while(alive&&generation==scanGeneration&&SystemClock.elapsedRealtime()<end){long now=SystemClock.elapsedRealtime();if(now>=next){for(String target:targets)try{socket.send(new DatagramPacket(probe,probe.length,InetAddress.getByName(target),42107));}catch(IOException ignored){}next=now+900;}
       try{DatagramPacket packet=new DatagramPacket(new byte[512],512);socket.receive(packet);String host=packet.getAddress().getHostAddress();JSONObject j=new JSONObject(new String(packet.getData(),0,packet.getLength(),StandardCharsets.UTF_8));String id=j.optString("id"),name=j.optString("name");if("xiaobai".equals(j.optString("service"))&&j.optInt("protocol")==1&&j.optInt("port")==80&&id.matches("[a-f0-9]{12}")&&name.length()>0&&name.length()<=48&&privateIp(host)){Device d=new Device(id,name,host);runOnUiThread(()->{if(alive&&generation==scanGeneration&&web==null){devices.put(id,d);renderDevices();}});}}
       catch(SocketTimeoutException ignored){}catch(JSONException ignored){}
      }
    }
   }catch(Exception e){failure="未能完成搜索，请检查 Wi-Fi，或手动输入 IP。";}finally{activeSocket=null;if(lock!=null&&lock.isHeld())lock.release();scanning=false;}
   final String error=failure;runOnUiThread(()->{if(alive&&generation==scanGeneration&&web==null)message.setText(error!=null?error:devices.isEmpty()?"暂未找到设备。请确认小白已开机并连接相同 Wi-Fi。":"找到 "+devices.size()+" 台小白，选择一台连接。");});
  });
 }
 private void manual(){EditText input=new EditText(this);input.setSingleLine();input.setHint("例如 192.168.4.1");input.setText(getPreferences(0).getString("last_host","192.168.4.1"));input.setInputType(android.text.InputType.TYPE_CLASS_PHONE);new AlertDialog.Builder(this).setTitle("输入小白的局域网 IP").setView(input).setPositiveButton("连接",(d,w)->verifyAndOpen(input.getText().toString().trim(),null)).setNegativeButton("取消",null).show();}
 private void verifyAndOpen(String host,String expectedId){if(!privateIp(host)){toast("请输入有效的局域网 IPv4 地址");return;}Network network=findWifi();if(network==null){toast("请先连接 Wi-Fi");return;}cancelScan();final int request=++connectionGeneration;message.setText("正在连接 "+host+"…");worker.execute(()->{try{
   HttpURLConnection c=(HttpURLConnection)network.openConnection(new URL("http://"+host+"/api/status"));c.setConnectTimeout(2500);c.setReadTimeout(2500);c.setInstanceFollowRedirects(false);JSONObject j;
   try{if(c.getResponseCode()!=200)throw new IOException();try(InputStream in=c.getInputStream();ByteArrayOutputStream out=new ByteArrayOutputStream()){byte[] bytes=new byte[2048];int count;while((count=in.read(bytes))!=-1){if(out.size()+count>16384)throw new IOException();out.write(bytes,0,count);}j=new JSONObject(out.toString("UTF-8"));}}finally{c.disconnect();}
   if(!j.optString("device_id").matches("[a-f0-9]{12}")||!j.optString("version").startsWith("XiaoBai ")||(expectedId!=null&&!expectedId.equals(j.optString("device_id"))))throw new IOException();String name=j.optString("device_name","小白");runOnUiThread(()->{if(alive&&request==connectionGeneration){wifi=network;openDevice(host,name);}});
  }catch(Exception e){runOnUiThread(()->{if(alive&&web==null&&request==connectionGeneration){message.setText("连接失败：检查 IP、Wi-Fi 与设备隔离设置。");toast("小白暂时无法连接");}});}});
 }
 private void openDevice(String host,String name){cancelScan();closeWeb();currentHost=host;connectivity.bindProcessToNetwork(wifi);getPreferences(0).edit().putString("last_host",host).apply();createRoot();root.setTag("controller");root.setPadding(0,0,0,0);root.requestApplyInsets();
  LinearLayout bar=new LinearLayout(this);bar.setGravity(Gravity.CENTER_VERTICAL);Button back=button("‹ 设备",this::showHome);bar.addView(back,new LinearLayout.LayoutParams(dp(85),dp(48)));TextView title=label(name,17,true);bar.addView(title,new LinearLayout.LayoutParams(0,-2,1));root.addView(bar);
  web=new WebView(this);web.resumeTimers();web.setBackgroundColor(themes[theme][0]);WebSettings s=web.getSettings();s.setJavaScriptEnabled(true);s.setDomStorageEnabled(true);s.setAllowFileAccess(false);s.setAllowContentAccess(false);s.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);s.setSupportMultipleWindows(false);s.setMediaPlaybackRequiresUserGesture(true);s.setUserAgentString(s.getUserAgentString()+" XiaoBaiAndroid/0.9");
  web.setWebChromeClient(new WebChromeClient());
  web.setWebViewClient(new WebViewClient(){private boolean allowed(Uri uri){return "http".equals(uri.getScheme())&&currentHost.equals(uri.getHost())&&(uri.getPort()==-1||uri.getPort()==80||uri.getPort()==81);}
   @Override public boolean shouldOverrideUrlLoading(WebView v,WebResourceRequest r){return !allowed(r.getUrl())||r.getUrl().getPath().endsWith(".apk");}
   @Override public WebResourceResponse shouldInterceptRequest(WebView v,WebResourceRequest r){return allowed(r.getUrl())||"data".equals(r.getUrl().getScheme())?null:new WebResourceResponse("text/plain","UTF-8",new ByteArrayInputStream(new byte[0]));}
   @Override public void onReceivedError(WebView v,WebResourceRequest r,WebResourceError e){if(r.isForMainFrame())toast("连接中断，履带会自动停车；可返回重新连接。");}
  });root.addView(web,new LinearLayout.LayoutParams(-1,0,1));web.loadUrl("http://"+host+"/");getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
 }
 private void cancelScan(){scanGeneration++;DatagramSocket socket=activeSocket;if(socket!=null)socket.close();}
 private void closeWeb(){if(web!=null){WebView old=web;web=null;old.stopLoading();old.loadUrl("about:blank");old.removeAllViews();old.destroy();}getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);}
 private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_LONG).show();}
 @Override public void onBackPressed(){if(web!=null)showHome();else super.onBackPressed();}
 @Override protected void onPause(){super.onPause();++connectionGeneration;cancelScan();if(web!=null){web.evaluateJavascript("typeof disconnect==='function'&&disconnect()",null);web.onPause();web.pauseTimers();}}
 @Override protected void onResume(){super.onResume();if(web!=null){web.resumeTimers();web.onResume();web.evaluateJavascript("typeof poll==='function'&&poll()",null);}}
 @Override public void onConfigurationChanged(Configuration c){super.onConfigurationChanged(c);if(web==null)showHome();}
 @Override protected void onDestroy(){alive=false;cancelScan();closeWeb();connectivity.bindProcessToNetwork(null);worker.shutdownNow();super.onDestroy();}
 private final class RobotView extends View {
  private final Paint p=new Paint(Paint.ANTI_ALIAS_FLAG);private ValueAnimator animator;private float phase=0;
  RobotView(Context c){super(c);setContentDescription("小白，大屏幕与会眨动的双眼");}
  private void updateMotion(){boolean active=isAttachedToWindow()&&getWindowVisibility()==VISIBLE&&hasWindowFocus()&&ValueAnimator.areAnimatorsEnabled();if(active&&animator==null){animator=ValueAnimator.ofFloat(0,1);animator.setDuration(4800);animator.setRepeatCount(ValueAnimator.INFINITE);animator.setInterpolator(new android.view.animation.LinearInterpolator());animator.addUpdateListener(a->{phase=(float)a.getAnimatedValue();invalidate();});animator.start();}else if(!active&&animator!=null){animator.cancel();animator=null;}}
  @Override protected void onAttachedToWindow(){super.onAttachedToWindow();updateMotion();}
  @Override protected void onDetachedFromWindow(){if(animator!=null){animator.cancel();animator=null;}super.onDetachedFromWindow();}
  @Override protected void onWindowVisibilityChanged(int visibility){super.onWindowVisibilityChanged(visibility);updateMotion();}
  @Override public void onWindowFocusChanged(boolean focus){super.onWindowFocusChanged(focus);updateMotion();}
  private void box(Canvas c,float x,float y,float w,float h,float r,int color){p.setColor(color);c.drawRoundRect(x,y,x+w,y+h,r,r,p);}
  @Override protected void onDraw(Canvas c){super.onDraw(c);float scale=Math.min(getWidth()/300f,getHeight()/200f);c.save();c.translate((getWidth()-300*scale)/2,(getHeight()-200*scale)/2);c.scale(scale,scale);p.setColor(0x120c3126);c.drawOval(52,173,248,188,p);c.translate(0,(float)Math.sin(phase*Math.PI*2)*1.8f);box(c,36,14,228,154,36,theme==5?0xff8495a8:0xfffcfff9);box(c,46,24,208,134,29,0xff1b2b37);float blink=Math.abs(phase-.85f)<.032f?Math.max(.08f,Math.abs(phase-.85f)/.032f):1;for(int i=0;i<2;i++)box(c,76+i*100,91-39*blink,48,78*blink,23,0xffdcfff3);c.restore();}
 }
}
