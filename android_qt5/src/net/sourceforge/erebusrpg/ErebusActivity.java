package net.sourceforge.erebusrpg;

import android.content.Context;
import android.media.AudioManager;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.KeyEvent;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;
//import android.widget.Toast;

import org.qtproject.qt5.android.bindings.QtActivity;

public class ErebusActivity extends QtActivity
{
    public static native void AndroidOnPause();
    public static native void AndroidOnResume();
    public static native void AndroidSetLargeScreen();
    private boolean large_screen = false;

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        // keep screen active - see http://stackoverflow.com/questions/2131948/force-screen-on
        getWindow().addFlags(LayoutParams.FLAG_KEEP_SCREEN_ON);

        {
            Display display = getWindowManager().getDefaultDisplay();
            DisplayMetrics outMetrics = new DisplayMetrics ();
            display.getMetrics(outMetrics);

            float density  = getResources().getDisplayMetrics().density;
            float dpWidth  = outMetrics.widthPixels / density;
            float dpHeight = outMetrics.heightPixels / density;
            //Toast.makeText(this, "Screen size dp " + dpWidth + " x " + dpHeight, Toast.LENGTH_LONG).show();
            large_screen = dpWidth >= 960;
            // this code is called before we load the native library, so we don't call the native function yet
        }
        /*int screenSize = getResources().getConfiguration().screenLayout &
                Configuration.SCREENLAYOUT_SIZE_MASK;

        switch(screenSize) {
            case Configuration.SCREENLAYOUT_SIZE_LARGE:
                Toast.makeText(this, "Large screen",Toast.LENGTH_LONG).show();
                break;
            case Configuration.SCREENLAYOUT_SIZE_NORMAL:
                Toast.makeText(this, "Normal screen",Toast.LENGTH_LONG).show();
                break;
            case Configuration.SCREENLAYOUT_SIZE_SMALL:
                Toast.makeText(this, "Small screen",Toast.LENGTH_LONG).show();
                break;
            default:
                Toast.makeText(this, "Screen size is neither large, normal or small" , Toast.LENGTH_LONG).show();
        }*/
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event)
    {
        switch (keyCode) {
            case KeyEvent.KEYCODE_VOLUME_UP:
            {
                AudioManager audio = (AudioManager)getSystemService(Context.AUDIO_SERVICE);
                if( audio != null ) {
                    System.out.println("audio up");
                    audio.adjustStreamVolume(AudioManager.STREAM_MUSIC, AudioManager.ADJUST_RAISE, AudioManager.FLAG_SHOW_UI);
                }
                else {
                    System.out.println("audio up - null");
                }
                return true;
            }
            case KeyEvent.KEYCODE_VOLUME_DOWN:
            {
                AudioManager audio = (AudioManager)getSystemService(Context.AUDIO_SERVICE);
                if( audio != null ) {
                    System.out.println("audio down");
                    audio.adjustStreamVolume(AudioManager.STREAM_MUSIC, AudioManager.ADJUST_LOWER, AudioManager.FLAG_SHOW_UI);
                }
                else {
                    System.out.println("audio down - null");
                }
                return true;
            }
        }

		return super.onKeyDown(keyCode, event);
    }

    @Override
    protected void onPause()
    {
        super.onPause();
		AndroidOnPause();
    }

    @Override
    protected void onResume()
    {
        super.onResume();
		AndroidOnResume();
    }

}
