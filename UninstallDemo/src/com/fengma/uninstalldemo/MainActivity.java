package com.fengma.uninstalldemo;

import java.lang.reflect.Method;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;

public class MainActivity extends Activity {

    static {
        System.loadLibrary("UninstallDemo");
    }

    private void initProcess(String userId){
        String appDir = getApplicationInfo().dataDir;
        String appFilesDir = getFilesDir().getPath();
        String appObservedFile = appFilesDir + "/sharedprefs";
        String appLockFile = appFilesDir + "/lockFile";
        init(userId, appDir, appFilesDir, appObservedFile, appLockFile , "http://www.wumii.com");
    }

    private native void init(String userId, String appDir, String appFilesDir, String appObservedFile, String apLockFile, String url);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        //aip>=17 增加userSerial的概念，在此处区分
        if(Build.VERSION.SDK_INT < 17){
            initProcess(null);
        }else{
            String userSerial = getUserSerial();
            initProcess(userSerial);
        }
    }
    // 利用反射获取userSerial
    private String getUserSerial()
    {
        Object userManager = getSystemService("user");
        if (userManager == null)
        {
            return null;
        }
        try
        {
            Method myUserHandleMethod = android.os.Process.class.getMethod("myUserHandle", (Class<?>[]) null);
            Object myUserHandle = myUserHandleMethod.invoke(android.os.Process.class, (Object[]) null);
            Method getSerialNumberForUser = userManager.getClass().getMethod("getSerialNumberForUser", myUserHandle.getClass());
            long userSerial = (Long) getSerialNumberForUser.invoke(userManager, myUserHandle);
            return String.valueOf(userSerial);
        }catch(Exception e){
            e.printStackTrace();
        }
        return null;
    }
}
