// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.ConditionVariable;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * Wrapper class to start a Quic test server.
 */
@JNINamespace("cronet")
public final class QuicTestServer {
    private static final ConditionVariable sBlock = new ConditionVariable();

    public static void startQuicTestServer(Context context) {
        nativeStartQuicTestServer(TestFilesInstaller.getInstalledPath(context));
        sBlock.block();
    }

    public static void shutdownQuicTestServer() {
        nativeShutdownQuicTestServer();
        sBlock.close();
    }

    public static String getServerURL() {
        return "http://" + getServerHost() + ":" + getServerPort();
    }

    public static String getServerHost() {
        return nativeGetServerHost();
    }

    public static int getServerPort() {
        return nativeGetServerPort();
    }

    @CalledByNative
    private void onServerStarted() {
        sBlock.open();
    }

    private static native void nativeStartQuicTestServer(String filePath);
    private static native void nativeShutdownQuicTestServer();
    private static native String nativeGetServerHost();
    private static native int nativeGetServerPort();
}
