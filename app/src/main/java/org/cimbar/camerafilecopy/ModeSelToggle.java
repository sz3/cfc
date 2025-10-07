package org.cimbar.camerafilecopy;

import android.content.Context;
import android.util.AttributeSet;
import androidx.appcompat.widget.AppCompatToggleButton;

public class ModeSelToggle extends AppCompatToggleButton {

    private static final int[] STATE_MODE4C = { R.attr.state_mode4c };
    private static final int[] STATE_MODEBM = { R.attr.state_modebm };

    private int mModeVal = 0;

    public ModeSelToggle(Context context) {
        super(context);
    }

    public ModeSelToggle(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public ModeSelToggle(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    // setter!
    public void setModeVal(int modeval) {
        if (mModeVal != modeval) {
            mModeVal = modeval;
            // Force the view to re-evaluate its drawable state
            refreshDrawableState(); 
        }
    }

    @Override
    protected int[] onCreateDrawableState(int extraSpace) {
        // 1. Get the current standard drawable states (includes state_checked)
        final int[] drawableState = super.onCreateDrawableState(extraSpace + 1);

        // we only care when the checkbox is checked
        if (isChecked()) {
            switch (mModeVal) {
                case 4:
                    mergeDrawableStates(drawableState, STATE_MODE4C);
                    break;
                case 67:
                    mergeDrawableStates(drawableState, STATE_MODEBM);
                    break;
                case 68:
                default:
                    // no-op for default mode (B)
                    break;
            }
        }

        return drawableState;
    }
}
