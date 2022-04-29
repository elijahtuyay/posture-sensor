package com.example.posturesensor.ui;

import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.cardview.widget.CardView;
import androidx.core.content.ContextCompat;

import com.android.volley.AuthFailureError;
import com.android.volley.NetworkError;
import com.android.volley.ParseError;
import com.android.volley.Request;
import com.android.volley.ServerError;
import com.android.volley.TimeoutError;
import com.android.volley.toolbox.JsonObjectRequest;
import com.example.posturesensor.R;

import org.json.JSONException;
import org.json.JSONObject;

import java.lang.ref.WeakReference;

public class PostureActivity extends AppCompatActivity {
    private final String CHANNEL_URL = " ";
    private final int DELAY = 10;

    TextView slouchNum, leanNum, leftNum, rightNum, overall;
    CardView slouchCard, leanCard, leftCard, rightCard;
    Drawable backgroundCircle;

    private final MyHandler handler = new MyHandler(this);
    private static Runnable scrape;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        slouchNum = findViewById(R.id.slouch_number);
        leanNum = findViewById(R.id.lean_number);
        leftNum = findViewById(R.id.left_skew_number);
        rightNum = findViewById(R.id.right_skew_number);

        slouchCard = findViewById(R.id.slouch_card);
        leanCard = findViewById(R.id.lean_card);
        leftCard = findViewById(R.id.left_skew_card);
        rightCard = findViewById(R.id.right_skew_card);

        overall = findViewById(R.id.overall);
        backgroundCircle = overall.getBackground();
    }

    @Override
    protected void onResume() {
        super.onResume();

        connect();

        // call every DELAY seconds
        scrape = () -> {
            connect();
            handler.postDelayed(scrape, 1000 * DELAY);
        };
        handler.post(scrape);
    }

    @Override
    protected void onPause() {
        super.onPause();
        clean();
    }

    // insurance for force-quits; technically onPause should be enough for regular use
    @Override
    protected void onDestroy() {
        super.onDestroy();
        clean();
    }

    /**
     * method that handles Volley networking call and all auxiliary sub-functions
     */
    private void connect() {
        MySingleton.getInstance(this.getApplicationContext()).getRequestQueue().cancelAll(request -> true);
        MySingleton.getInstance(this.getApplicationContext()).getRequestQueue().getCache().clear();

        JsonObjectRequest collectData = new JsonObjectRequest(
                Request.Method.GET,
                CHANNEL_URL, // TODO: change appropriate url
                null,
                response -> {
                    try {
                        JSONObject myResponse = new JSONObject(response.toString());

                        String slouchVal = myResponse.getString("field1");
                        String leanVal = myResponse.getString("field2");
                        String leftVal = myResponse.getString("field3");
                        String rightVal = myResponse.getString("field4");
                        String overallVal = myResponse.getString("field5");

                        textSetter(slouchVal, slouchNum, slouchCard);
                        textSetter(leanVal, leanNum, leanCard);
                        textSetter(leftVal, leftNum, leftCard);
                        textSetter(rightVal, rightNum, rightCard);

                        circleSetter(overallVal);

                    } catch (JSONException e) { // what if response is null?
                        e.printStackTrace();
                    }
                },
                error -> {
                    if (error instanceof AuthFailureError) {
                        Toast.makeText(getApplicationContext(), "Authentication failed.", Toast.LENGTH_SHORT).show();
                    } else if (error instanceof NetworkError) {
                        Toast.makeText(getApplicationContext(), "Could not reach the server. Please check your Internet connection, firewall or VPN settings.", Toast.LENGTH_SHORT).show();
                    } else if (error instanceof ParseError) {
                        Toast.makeText(getApplicationContext(), "Cannot parse server response data.", Toast.LENGTH_SHORT).show();
                    } else if (error instanceof ServerError) {
                        Toast.makeText(getApplicationContext(), "Server appears to be down.", Toast.LENGTH_SHORT).show();
                    } else if (error instanceof TimeoutError) {
                        Toast.makeText(getApplicationContext(), "Took too long for server to respond.", Toast.LENGTH_SHORT).show();
                    } else {
                        Toast.makeText(getApplicationContext(), "We have never seen this issue. Shutting down for safety.", Toast.LENGTH_SHORT).show();
                        finishAndRemoveTask();
                    }
                }
        );
        MySingleton.getInstance(this.getApplicationContext()).addToRequestQueue(collectData);
    }

    /**
     * sets the text and card coloring for the four individual sensor values
     * @param field    directly retrieved JSON String
     * @param textView chosen textview to set the value of
     * @param cardView chosen cardview to change the color of
     */
    private void textSetter(String field, TextView textView, CardView cardView) {
        double myVal;

        textView.setText(field);
        myVal = Double.parseDouble(field);

        if (myVal >= 0 && myVal < 20) {
            cardView.setCardBackgroundColor(ContextCompat.getColor(this, R.color.red));
        } else if (myVal >= 20 && myVal < 40) {
            cardView.setCardBackgroundColor(ContextCompat.getColor(this, R.color.orange));
        } else if (myVal >= 40 && myVal < 60) {
            cardView.setCardBackgroundColor(ContextCompat.getColor(this, R.color.yellow));
        } else if (myVal >= 60 && myVal < 80) {
            cardView.setCardBackgroundColor(ContextCompat.getColor(this, R.color.yellow_green));
        } else if (myVal >= 80 && myVal <= 100) {
            cardView.setCardBackgroundColor(ContextCompat.getColor(this, R.color.green));
        } else {
            cardView.setCardBackgroundColor(ContextCompat.getColor(this, R.color.gray));
        }
    }

    /**
     * sets the text and circle coloring for the overall value
     * @param field directly retrieved 5th JSON String (computed average)
     */
    private void circleSetter(String field) {
        double myVal;

        overall.setText(field);
        myVal = Double.parseDouble(field);

        if (myVal >= 0 && myVal < 20) {
            overall.setBackgroundResource(R.color.red);
        } else if (myVal >= 20 && myVal < 40) {
            overall.setBackgroundResource(R.color.orange);
        } else if (myVal >= 40 && myVal < 60) {
            overall.setBackgroundResource(R.color.yellow);
        } else if (myVal >= 60 && myVal < 80) {
            overall.setBackgroundResource(R.color.yellow_green);
        } else if (myVal >= 80 && myVal <= 100) {
            overall.setBackgroundResource(R.color.green);
        } else {
            overall.setBackgroundResource(R.color.gray);
        }
    }

    /**
     * removes messages and network requests in queue
     */
    private void clean() {
        MySingleton.getInstance(this.getApplicationContext()).getRequestQueue().cancelAll(request -> true);
        MySingleton.getInstance(this.getApplicationContext()).getRequestQueue().getCache().clear();

        handler.removeCallbacksAndMessages(null);
        handler.removeCallbacks(scrape);
        scrape = null;
    }

    /**
     * custom handler class for network implementation
     */
    private static class MyHandler extends Handler {
        private final WeakReference<PostureActivity> mActivity;

        public MyHandler(PostureActivity activity) {
            mActivity = new WeakReference<>(activity);
        }

        @Override
        public void handleMessage(Message msg) {
            PostureActivity activity = mActivity.get();
            if (activity != null) {
                this.handleMessage(msg);
            }
        }
    }
}


