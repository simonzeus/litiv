#pragma once

#include "BackgroundSubtractorLBSP.h"

//! defines the default value for BackgroundSubtractorLBSP::m_fRelLBSPThreshold
#define BGSSUBSENSE_DEFAULT_LBSP_REL_SIMILARITY_THRESHOLD (0.300f)
//! defines the default value for BackgroundSubtractorLBSP::m_nLBSPThresholdOffset
#define BGSSUBSENSE_DEFAULT_LBSP_OFFSET_SIMILARITY_THRESHOLD (0)
//! defines the default value for BackgroundSubtractorLBSP::m_nDescDistThreshold
#define BGSSUBSENSE_DEFAULT_DESC_DIST_THRESHOLD (3)
//! defines the default value for BackgroundSubtractorSuBSENSE::m_nMinColorDistThreshold
#define BGSSUBSENSE_DEFAULT_COLOR_DIST_THRESHOLD (30)
//! defines the default value for BackgroundSubtractorSuBSENSE::m_nBGSamples
//#define BGSSUBSENSE_DEFAULT_NB_BG_SAMPLES (50)
#define BGSSUBSENSE_DEFAULT_NB_BG_SAMPLES (35)
//! defines the default value for BackgroundSubtractorSuBSENSE::m_nRequiredBGSamples
#define BGSSUBSENSE_DEFAULT_REQUIRED_NB_BG_SAMPLES (2)
//! defines the number of samples to use when computing running averages
#define BGSSUBSENSE_N_SAMPLES_FOR_ST_MVAVGS (25)
#define BGSSUBSENSE_N_SAMPLES_FOR_LT_MVAVGS (100)
//! defines the threshold value(s) used to detect long-term ghosting and trigger a fast edge-based absorption in the model
#define BGSSUBSENSE_GHOST_DETECTION_D_MAX (0.010f)
#define BGSSUBSENSE_GHOST_DETECTION_S_MIN (0.995f)
//! defines the threshold value(s) used to detect high variation regions that are often labelled as foreground and trigger a local, gradual change in distance thresholds
#define BGSSUBSENSE_HIGH_VAR_DETECTION_S_MIN (0.850f)
#define BGSSUBSENSE_HIGH_VAR_DETECTION_D_MIN (0.175f)
#define BGSSUBSENSE_HIGH_VAR_DETECTION_S_MIN2 (0.100f)
#define BGSSUBSENSE_HIGH_VAR_DETECTION_D_MIN2 (0.225f)
//! defines the threshold value(s) used to detect unstable regions and edges
#define BGSSUBSENSE_INSTBLTY_DETECTION_SEGM_DIFF (0.150f)
#define BGSSUBSENSE_INSTBLTY_DETECTION_MIN_R_VAL (3.000f)
//! parameter(s) used for dynamic distance threshold adjustments ('R(x)')
#define BGSSUBSENSE_R_VAR (0.01f)
//! parameter(s) used for adjusting the variation speed of dynamic distance thresholds  ('R2(x)')
#define BGSSUBSENSE_R2_INCR  (1.000f)
#define BGSSUBSENSE_R2_DECR  (0.100f)
//! parameter(s) used for dynamic learning rates adjustments  ('T(x)')
#define BGSSUBSENSE_T_DECR  (0.2500f)
#define BGSSUBSENSE_T_INCR  (0.5000f)
#define BGSSUBSENSE_T_LOWER (2.0000f)
#define BGSSUBSENSE_T_UPPER (256.00f)

/*!
	Self-Balanced Sensitivity segmenTER (SuBSENSE) foreground-background segmentation algorithm.

	Note: both grayscale and RGB/BGR images may be used with this extractor (parameters are adjusted automatically).
	For optimal grayscale results, use CV_8UC1 frames instead of CV_8UC3.

	For more details on the different parameters, go to @@@@@@@@@@@@@@.

	This algorithm is currently NOT thread-safe.
 */
class BackgroundSubtractorSuBSENSE : public BackgroundSubtractorLBSP {
public:
	//! full constructor
	BackgroundSubtractorSuBSENSE(	float fRelLBSPThreshold=BGSSUBSENSE_DEFAULT_LBSP_REL_SIMILARITY_THRESHOLD,
									size_t nLBSPThresholdOffset=BGSSUBSENSE_DEFAULT_LBSP_OFFSET_SIMILARITY_THRESHOLD,
									size_t nMinDescDistThreshold=BGSSUBSENSE_DEFAULT_DESC_DIST_THRESHOLD,
									size_t nMinColorDistThreshold=BGSSUBSENSE_DEFAULT_COLOR_DIST_THRESHOLD,
									size_t nBGSamples=BGSSUBSENSE_DEFAULT_NB_BG_SAMPLES,
									size_t nRequiredBGSamples=BGSSUBSENSE_DEFAULT_REQUIRED_NB_BG_SAMPLES);
	//! default destructor
	virtual ~BackgroundSubtractorSuBSENSE();
	//! (re)initiaization method; needs to be called before starting background subtraction (note: also reinitializes the keypoints vector)
	virtual void initialize(const cv::Mat& oInitImg, const std::vector<cv::KeyPoint>& voKeyPoints);
	//! refreshes all samples based on the last analyzed frame
	virtual void refreshModel(float fSamplesRefreshFrac);
	//! primary model update function; the learning param is used to override the internal learning thresholds (ignored when <= 0)
	virtual void operator()(cv::InputArray image, cv::OutputArray fgmask, double learningRateOverride=0);
	//! returns a copy of the latest reconstructed background image
	void getBackgroundImage(cv::OutputArray backgroundImage) const;

protected:
	//! indicates whether internal structures have already been initialized (LBSP lookup tables, samples, etc.)
	bool m_bInitializedInternalStructs;
	//! absolute minimal color distance threshold ('R' or 'radius' in the original ViBe paper, used as the default/initial 'R(x)' value here, paired with BackgroundSubtractorLBSP::m_nDescDistThreshold)
	const size_t m_nMinColorDistThreshold;
	//! number of different samples per pixel/block to be taken from input frames to build the background model (same as 'N' in ViBe/PBAS)
	const size_t m_nBGSamples;
	//! number of similar samples needed to consider the current pixel/block as 'background' (same as '#_min' in ViBe/PBAS)
	const size_t m_nRequiredBGSamples;
	//! current frame index
	size_t m_nFrameIndex;
	//! last calculated mean color diff ratio
	float m_fLastColorDiffRatio;
	//! current model reset cooldown timer
	size_t m_nModelResetCooldown;
	//! current learning rate caps
	size_t m_nCurrLearningRateLowerCap, m_nCurrLearningRateUpperCap;
	//! current kernel size for median blur post-proc filtering
	int m_nMedianBlurKernelSize;
	//! specifies the px update spread range
	bool m_bUse3x3Spread;

	//! background model pixel color intensity samples (equivalent to 'B(x)' in PBAS, but also paired with BackgroundSubtractorLBSP::m_voBGDescSamples to create our complete model)
	std::vector<cv::Mat> m_voBGColorSamples;

	//! per-pixel update rates ('T(x)' in PBAS, which contains pixel-level 'sigmas', as referred to in ViBe)
	cv::Mat m_oUpdateRateFrame;
	//! per-pixel distance thresholds (equivalent to 'R(x)' in PBAS, but used as a relative value to determine both intensity and descriptor variation thresholds)
	cv::Mat m_oDistThresholdFrame;
	//! per-pixel distance threshold variation modulators ('R2(x)', relative value used to modulate 'R(x)' variations)
	cv::Mat m_oDistThresholdVariationFrame;
	//! per-pixel mean minimal distances from the model ('D_min(x)' in PBAS, used to control variation magnitude and direction of 'T(x)' and 'R(x)')
	cv::Mat m_oMeanMinDistFrame_LT, m_oMeanMinDistFrame_ST;
	//! per-pixel mean distances between consecutive frames ('D_last(x)', used to detect ghosts and high variation regions in the sequence)
	cv::Mat m_oMeanLastDistFrame_LT, m_oMeanLastDistFrame_ST;
	//! per-pixel mean downsampled distances between consecutive frames (used to analyze camera movement and control max learning rates globally)
	cv::Mat m_oMeanDownSampledLastDistFrame_LT, m_oMeanDownSampledLastDistFrame_ST;
	//! per-pixel mean raw segmentation results
	cv::Mat m_oMeanRawSegmResFrame_LT, m_oMeanRawSegmResFrame_ST;
	//! per-pixel mean final segmentation results
	cv::Mat m_oMeanFinalSegmResFrame_LT, m_oMeanFinalSegmResFrame_ST;
	//! a lookup map used to keep track of unstable regions
	cv::Mat m_oUnstableRegionMask;
	//! per-pixel blink detection results ('Z(x)', used to determine which frame regions should be assigned stronger 'R(x)' variations)
	cv::Mat m_oBlinksFrame;
	//! pre-allocated matrix used to downsample (1/8) the input frame when needed
	cv::Mat m_oDownSampledColorFrame;
	//! copy of previously used pixel intensities used to calculate 'D_last(x)'
	cv::Mat m_oLastColorFrame;
	//! copy of previously used descriptors used to calculate 'D_last(x)'
	cv::Mat m_oLastDescFrame;
	//! the foreground mask generated by the method at [t-1] (without post-proc, used for blinking px detection)
	cv::Mat m_oRawFGMask_last;
	//! the foreground mask generated by the method at [t-1] (with post-proc)
	cv::Mat m_oFGMask_last;

	//! pre-allocated CV_8UC1 matrices used to speed up morph ops
	cv::Mat m_oFGMask_PreFlood;
	cv::Mat m_oFGMask_FloodedHoles;
	cv::Mat m_oFGMask_last_dilated;
	cv::Mat m_oFGMask_last_dilated_inverted;
	cv::Mat m_oRawFGBlinkMask_curr;
	cv::Mat m_oRawFGBlinkMask_last;
};
