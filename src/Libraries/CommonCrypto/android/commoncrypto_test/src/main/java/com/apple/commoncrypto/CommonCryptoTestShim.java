package com.apple.commoncrypto;

public final class CommonCryptoTestShim {

    static {
        System.loadLibrary("commoncrypto_test");
    }

    private CommonCryptoTestShim() {
    }

    public static native int startTest();

}
