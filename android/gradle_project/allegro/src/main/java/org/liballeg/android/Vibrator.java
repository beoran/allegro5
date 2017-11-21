package org.liballeg.android;

import android.app.Activity;

import android.content.ClipboardManager;
import android.util.Log;


class Vibrator
{
   private static final String TAG = "Vibrator";

   private Activity            activity;
   private android.os.Vibrator vibrator;   

   Vibrator(Activity activity)
   {
      this.activity = activity;
      this.vibrator = 
      (android.os.Vibrator) activity.getSystemService(android.os.Vibrator.class);
   }
   
   public void vibrate(int milliseconds)
   {
      this.vibrator.vibrate(milliseconds);
   }

   public void cancel()
   {
      this.vibrator.cancel();
   } 
   
   public boolean hasVibrator() {
      return this.vibrator.hasVibrator();
   }
}

/* vim: set sts=3 sw=3 et: */
