package com.apple.commoncrypto;

import android.support.test.runner.AndroidJUnit4;

import junit.framework.TestCase;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class CommonCryptoTest {

    @Test
    public void startTest() {
        if (CommonCryptoTestShim.startTest() != 0) {
            TestCase.fail();
        }
    }
}
