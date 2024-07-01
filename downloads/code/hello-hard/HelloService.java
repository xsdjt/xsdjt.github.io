package com.android.server;

import android.content.Context;
import android.os.IHelloService;
import android.util.Slog;

public class HelloService extends IHelloService.Stub {
private static final String TAG = "HelloService";

HelloService() 
{
    System.out.println("frameworks/base/services/java/com/android/server/HelloService.java HelloService()");
     init_native();
}

public void setVal(int val) 
{
    System.out.println("frameworks/base/services/java/com/android/server/HelloService.java setVal()");
    setVal_native(val);
}

public int getVal() 
{
    System.out.println("frameworks/base/services/java/com/android/server/HelloService.java getVal()");
    return getVal_native();
}

private static native boolean init_native();
private static native void setVal_native(int val);
private static native int getVal_native();

};
