
/*
 * http://ccrma.stanford.edu/software/snd/snd/clm.html#wave-train
 *
Here is a FOF instrument based loosely on fof.c of Perry Cook
and the article "Synthesis of the Singing Voice" by Bennett and Rodet in "Current Directions in Computer Music Research".

(definstrument fofins (beg dur frq amp vib f0 a0 f1 a1 f2 a2 &optional ve ae)
  (let* ((start (floor (* beg *srate*)))
         (end (+ start (floor (* dur *srate*))))
         (ampf (make-env :envelope (or ae (list 0 0 25 1 75 1 100 0)) :scaler amp :duration dur))
         (frq0 (hz->radians f0))
         (frq1 (hz->radians f1))
         (frq2 (hz->radians f2))
         (foflen (if (= *srate* 22050) 100 200))
         (vibr (make-oscil :frequency 6))
	 (vibenv (make-env :envelope (or ve (list 0 1 100 1)) :scaler vib :duration dur))
         (win-freq (/ two-pi foflen))
         (foftab (make-double-float-array foflen))
         (wt0 (make-wave-train :wave foftab :frequency frq)))
    (loop for i from 0 below foflen do
      (setf (aref foftab i) (double-float      
        ;; this is not the pulse shape used by B&R
            (* (+ (* a0 (sin (* i frq0))) 
                  (* a1 (sin (* i frq1))) 
                  (* a2 (sin (* i frq2)))) 
               .5 (- 1.0 (cos (* i win-freq)))))))
    (run
     (loop for i from start below end do
       (outa i (* (env ampf) (wave-train wt0 (* (env vibenv) (oscil vibr)))))))))


(with-sound () (fofins 0 1 270 .2 .001 730 .6 1090 .3 2440 .1)) ;"Ahh"

(with-sound () 
  (fofins 0 4 270 .2 0.005 730 .6 1090 .3 2440 .1 '(0 0 40 0 75 .2 100 1) 
          '(0 0 .5 1 3 .5 10 .2 20 .1 50 .1 60 .2 85 1 100 0))
  (fofins 0 4 (* 6/5 540) .2 0.005 730 .6 1090 .3 2440 .1 '(0 0 40 0 75 .2 100 1) 
          '(0 0 .5 .5 3 .25 6 .1 10 .1 50 .1 60 .2 85 1 100 0))
  (fofins 0 4 135 .2 0.005 730 .6 1090 .3 2440 .1 '(0 0 40 0 75 .2 100 1) 
          '(0 0 1 3 3 1 6 .2 10 .1 50 .1 60 .2 85 1 100 0)))

  */

/* map from stuff in the above fragment to C code below:
      a0,a1,a2  => amp0, amp1, amp2       The amplitudes of the fundamental frequencies.
      f0,f1,f2  => freq0, freq1, freq2    The amplitudes of the fundamental frequencies.

enum
{
  PARAM_FREQ0,
  PARAM_AMP0,
  PARAM_FREQ1,
  PARAM_AMP1,
  PARAM_FREQ2,
  PARAM_AMP2,
  PARAM_VIBRATO_DEPTH,
  PARAM_DURATION,                               /* BbDuration */
  PARAM_VIBRATO_ENVELOPE,                       /* BbWaveform */
  PARAM_AMPLITUDE_ENVELOPE,                     /* BbWaveform */
};

static BbInstrumentSimpleParam fofins_simple_params[] =
{
  { PARAM_FREQ0, "freq0", 730 },
  { PARAM_AMP0, "amp0", 0.6 },
  { PARAM_FREQ1, "freq1", 1090 },
  { PARAM_AMP1, "amp1", 0.3 },
  { PARAM_FREQ2, "freq2", 2440 },
  { PARAM_AMP2, "amp2", 0.1 },
  { PARAM_VIBRATO_DEPTH, "vibrato_depth", 0.005 },
};

static gdouble *
fofins_synth (BbInstrument *instrument,
              BbRenderInfo *render_info,
              GValue       *params,
              guint        *n_samples_out)
{
  gdouble freq0 = g_value_get_double (params + PARAM_FREQ0);
  gdouble freq1 = g_value_get_double (params + PARAM_FREQ1);
  gdouble freq2 = g_value_get_double (params + PARAM_FREQ2);
  gdouble amp0 = g_value_get_double (params + PARAM_AMP0);
  gdouble amp1 = g_value_get_double (params + PARAM_AMP1);
  gdouble amp2 = g_value_get_double (params + PARAM_AMP2);
  
