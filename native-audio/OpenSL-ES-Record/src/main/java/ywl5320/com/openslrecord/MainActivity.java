package ywl5320.com.openslrecord;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.os.AsyncTask;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;

import com.example.hellojnicallback.JniHandler;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.ShortBuffer;

import static android.media.AudioTrack.WRITE_BLOCKING;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
    }

    public native void rdSound(String path, short[] buffer);

    public native void rdStop();

    public void recourdsound(View view) {
//        new RecordTask().execute();
        rdSound(Environment.getExternalStorageDirectory().getAbsolutePath() + "/temp.pcm", JniHandler.getBuffer());
    }

    public void recourdstop(View view) {
//        isRecording = false;
        rdStop();
    }

    public void playAudio(View view){
        new PlayTask().execute();
    }

    private boolean isPlaying = false;
    private File audioFile = new File("/sdcard/temp.pcm");

    private static final int RECORDER_SAMPLE_RATE_LOW = 8000;
    private static final int RECORDER_SAMPLE_RATE_HIGH = 44100;
    private static final boolean isLow = false;

    // 录制频率，可以为8000hz或者11025hz等，不同的硬件设备这个值不同
    private static final int sampleRateInHz = isLow ? RECORDER_SAMPLE_RATE_LOW : RECORDER_SAMPLE_RATE_HIGH;
    // 录制通道
    private static final int channelConfig = AudioFormat.CHANNEL_CONFIGURATION_MONO;
//    private static final int channelConfig = 0x00000001 | 0x00000002;
    // 录制编码格式，可以为AudioFormat.ENCODING_16BIT和8BIT,其中16BIT的仿真性比8BIT好，但是需要消耗更多的电量和存储空间
    private static final int audioFormat = AudioFormat.ENCODING_PCM_16BIT;
    // 录制缓冲大小
    private static final int bufferSizeInByte = AudioRecord.getMinBufferSize(sampleRateInHz, channelConfig, audioFormat);

    private volatile boolean isRecording=false;

    class RecordTask extends AsyncTask<Void,Integer,Void>{

        @Override
        protected Void doInBackground(Void... voids) {
            try {
                if (isRecording)
                    return null;

                DataOutputStream dos = new DataOutputStream(new BufferedOutputStream(new FileOutputStream(audioFile)));

                AudioRecord audioRecord = new AudioRecord(MediaRecorder.AudioSource.MIC, sampleRateInHz, channelConfig, audioFormat, bufferSizeInByte);

                short[] buffer = new short[bufferSizeInByte];

                audioRecord.startRecording();
                isRecording = true;

                int r = 0;
                while (isRecording){
                    int size = audioRecord.read(buffer, 0, bufferSizeInByte);
                    for (int i = 0; i < size; i++){
                        dos.writeShort(buffer[i]);
                    }
                    r++;
                    publishProgress(r);
                }

                audioRecord.stop();
                Log.d("todo","停止录音. length: " + audioFile.length());
                dos.flush();
                dos.close();
            } catch (Exception e) {
                e.printStackTrace();
            }

            return null;
        }

        @Override
        protected void onProgressUpdate(Integer... values) {
            Log.d("todo","onProgressUpdate = " + values[0]);
        }
    }

    class PlayTask extends AsyncTask<Void,Integer,Void> {

        @Override
        protected Void doInBackground(Void... voids) {
            if (isPlaying)
                return null;

            Log.d("todo", "bufferSizeInByte: " + bufferSizeInByte);
            short[] buffer = new short[bufferSizeInByte >> 1];
            ByteBuffer byteBuffer = ByteBuffer.allocate(bufferSizeInByte);
//            byte[] bufferInByte = new byte[bufferSizeInByte];

            try {
                Log.v("todo", "准备播放的文件是："+audioFile.getPath());
                Log.v("todo", "准备播放的文件大小是："+audioFile.length());

                DataInputStream dis = new DataInputStream(new BufferedInputStream(new FileInputStream(audioFile)));
                // MediaPlayer无法播放pcm
                AudioTrack audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, sampleRateInHz, channelConfig, audioFormat, bufferSizeInByte, AudioTrack.MODE_STREAM);
                audioTrack.play();

                isPlaying = true;
                int sum = 0, read;

                while (((read = dis.read(byteBuffer.array(), 0, byteBuffer.limit())) != -1) && isPlaying) {
                    byteBuffer.clear();
                    int write = audioTrack.write(byteBuffer, read, WRITE_BLOCKING);
                    Log.i("todo", "audioTrack.write: " + write);
                }

//                while (isPlaying && dis.available() > 0){
//                    int i = 0;
//                    while (dis.available()>0 && i<buffer.length){
//                        buffer[i] = dis.readShort();
//                        i++;
//                    }
//                    audioTrack.write(buffer, 0, buffer.length);
//                    sum += buffer.length;
//                    Log.v("todo","写入到audioTrack的数据大小为：" + buffer.length +" 合计：" + sum);
//                }
                isPlaying = false;
                Log.v("todo","结束播放.");
                audioTrack.stop();
                audioTrack.release();
                dis.close();

            } catch (Exception e) {
                e.printStackTrace();
            }


            return null;
        }
    }
}
