
// @@@ imgproc gpu algo does not support mipmapping binding yet
// @@@ test compute shader group size vs shared mem usage
// @@@ add opencv hardware instr set check, and impl some stuff in MMX/SSE/SSE2/3/4.1/4.2? (also check popcount and AVX)
// @@@ support non-integer textures top level (alg)? need to replace all ui-stores by float-stores, rest is ok

//////////////////////////////////////////
// USER/ENVIRONMENT-SPECIFIC VARIABLES :
//////////////////////////////////////////
#define WRITE_BGSUB_IMG_OUTPUT           0
#define WRITE_BGSUB_AVI_OUTPUT           0
#define WRITE_BGSUB_METRICS_ANALYSIS     0
#define DISPLAY_BGSUB_DEBUG_OUTPUT       0
#define ENABLE_INTERNAL_TIMERS           0
#define ENABLE_DISPLAY_MOUSE_DEBUG       0
//////////////////////////////////////////
#define USE_VIBE_BGSUB                   0
#define USE_PBAS_BGSUB                   0
#define USE_PAWCS_BGSUB                  0
#define USE_LOBSTER_BGSUB                1
#define USE_SUBSENSE_BGSUB               0
//////////////////////////////////////////
#include "litiv/utils/ParallelUtils.hpp"
#if HAVE_GLSL
#define GLSL_EVALUATION                  1
#define VALIDATE_GLSL_EVALUATION         0
#endif //HAVE_GLSL
#include "litiv/utils/DatasetUtils.hpp"
#define DATASET_ID             eDataset_CDnet2014
#define DATASET_ROOT_PATH      std::string("/shared2/datasets/")
#define DATASET_RESULTS_PATH   std::string("results")
#define DATASET_PRECACHING     1
//////////////////////////////////////////
#if (HAVE_GLSL && GLSL_EVALUATION)
#if !(DATASET_ID==eDataset_CDnet2014 || DATASET_ID==eDataset_CDnet2012)
#error "GLSL eval can only be used for cdnet dataset"
#endif //!(DATASET_ID==eDataset_CDnet2014 || DATASET_ID==eDataset_CDnet2012)
#endif //(HAVE_GLSL && GLSL_EVALUATION)
#define NEED_LAST_GT_MASK (DISPLAY_BGSUB_DEBUG_OUTPUT || (WRITE_BGSUB_METRICS_ANALYSIS && (!GLSL_EVALUATION || VALIDATE_GLSL_EVALUATION)))
#define NEED_GT_MASK (DISPLAY_BGSUB_DEBUG_OUTPUT || WRITE_BGSUB_METRICS_ANALYSIS)
#define NEED_FG_MASK (DISPLAY_BGSUB_DEBUG_OUTPUT || WRITE_BGSUB_AVI_OUTPUT || WRITE_BGSUB_IMG_OUTPUT || ((!GLSL_EVALUATION || VALIDATE_GLSL_EVALUATION) && WRITE_BGSUB_METRICS_ANALYSIS))
#define LIMIT_MODEL_TO_SEQUENCE_ROI (USE_LOBSTER_BGSUB||USE_SUBSENSE_BGSUB||USE_PAWCS_BGSUB)
#define BOOTSTRAP_100_FIRST_FRAMES  (USE_LOBSTER_BGSUB||USE_SUBSENSE_BGSUB||USE_PAWCS_BGSUB)
#if (USE_LOBSTER_BGSUB+USE_SUBSENSE_BGSUB+USE_VIBE_BGSUB+USE_PBAS_BGSUB+USE_PAWCS_BGSUB)!=1
#error "Must specify a single algorithm."
#elif USE_VIBE_BGSUB
#include "litiv/video/BackgroundSubtractorViBe_1ch.hpp"
#include "litiv/video/BackgroundSubtractorViBe_3ch.hpp"
#elif USE_PBAS_BGSUB
#include "litiv/video/BackgroundSubtractorPBAS_1ch.hpp"
#include "litiv/video/BackgroundSubtractorPBAS_3ch.hpp"
#elif USE_PAWCS_BGSUB
#include "litiv/video/BackgroundSubtractorPAWCS.hpp"
#elif USE_LOBSTER_BGSUB
#include "litiv/video/BackgroundSubtractorLOBSTER.hpp"
#elif USE_SUBSENSE_BGSUB
#include "litiv/video/BackgroundSubtractorSuBSENSE.hpp"
#endif //USE_..._BGSUB
#if ENABLE_DISPLAY_MOUSE_DEBUG
#if (HAVE_GPU_SUPPORT || DEFAULT_NB_THREADS>1)
#error "Cannot support mouse debug with GPU support or with more than one thread."
#endif //(HAVE_GPU_SUPPORT || DEFAULT_NB_THREADS>1)
static int *pnLatestMouseX=nullptr, *pnLatestMouseY=nullptr;
void OnMouseEvent(int event, int x, int y, int, void*) {
    if(event!=cv::EVENT_MOUSEMOVE || !x || !y)
        return;
    *pnLatestMouseX = x;
    *pnLatestMouseY = y;
}
#endif //ENABLE_DISPLAY_MOUSE_DEBUG
#if WRITE_BGSUB_METRICS_ANALYSIS
cv::FileStorage g_oDebugFS;
#endif //!WRITE_BGSUB_METRICS_ANALYSIS
#if ENABLE_INTERNAL_TIMERS
#define TIMER_INTERNAL_TIC(x) TIMER_TIC(x)
#define TIMER_INTERNAL_TOC(x) TIMER_TOC(x)
#define TIMER_INTERNAL_ELAPSED_MS(x) TIMER_ELAPSED_MS(x)
#else //!ENABLE_INTERNAL_TIMERS
#define TIMER_INTERNAL_TIC(x)
#define TIMER_INTERNAL_TOC(x)
#define TIMER_INTERNAL_ELAPSED_MS(x)
#endif //!ENABLE_INTERNAL_TIMERS
#if WRITE_BGSUB_IMG_OUTPUT
const std::vector<int> g_vnResultsComprParams = {cv::IMWRITE_PNG_COMPRESSION,9}; // when writing output bin files, lower to increase processing speed
#endif //WRITE_BGSUB_IMG_OUTPUT
#if DISPLAY_BGSUB_DEBUG_OUTPUT
cv::Size g_oDisplayOutputSize(960,240);
bool g_bContinuousUpdates = false;
#endif //DISPLAY_BGSUB_DEBUG_OUTPUT
const DatasetUtils::Segm::Video::DatasetInfo& g_oDatasetInfo = DatasetUtils::Segm::Video::GetDatasetInfo(DatasetUtils::Segm::Video::DATASET_ID,DATASET_ROOT_PATH,DATASET_RESULTS_PATH,HAVE_GPU_SUPPORT);
#if (HAVE_GPU_SUPPORT && DEFAULT_NB_THREADS>1)
#warning "Cannot support multithreading + gpu exec, will keep one main thread + gpu instead"
#endif //(HAVE_GPU_SUPPORT && DEFAULT_NB_THREADS>1)
int AnalyzeSequence(int nThreadIdx, std::shared_ptr<DatasetUtils::Segm::Video::SequenceInfo> pCurrSequence, const std::string& sCurrResultsPath);
#if !HAVE_GPU_SUPPORT
const size_t g_nMaxThreads = DEFAULT_NB_THREADS;//std::thread::hardware_concurrency()>0?std::thread::hardware_concurrency():DEFAULT_NB_THREADS;
std::atomic_size_t g_nActiveThreads(0);
#endif //!HAVE_GPU_SUPPORT

int main() {
#if PLATFORM_USES_WIN32API
    SetConsoleWindowSize(80,40,1000);
#endif //PLATFORM_USES_WIN32API
    std::cout << "Parsing dataset..." << std::endl;
    std::vector<std::shared_ptr<DatasetUtils::Segm::Video::CategoryInfo>> vpCategories;
    for(auto oDatasetFolderPathIter=g_oDatasetInfo.vsFolderPaths.begin(); oDatasetFolderPathIter!=g_oDatasetInfo.vsFolderPaths.end(); ++oDatasetFolderPathIter) {
        try {
            vpCategories.push_back(std::make_shared<DatasetUtils::Segm::Video::CategoryInfo>(*oDatasetFolderPathIter,g_oDatasetInfo.sDatasetPath+*oDatasetFolderPathIter,g_oDatasetInfo));
        } catch(std::runtime_error& e) { std::cout << e.what() << std::endl; }
    }
    size_t nSeqTotal = 0;
    size_t nFramesTotal = 0;
    std::multimap<double,std::shared_ptr<DatasetUtils::Segm::Video::SequenceInfo>> mSeqLoads;
    for(auto pCurrCategory=vpCategories.begin(); pCurrCategory!=vpCategories.end(); ++pCurrCategory) {
        nSeqTotal += (*pCurrCategory)->m_vpSequences.size();
        for(auto pCurrSequence=(*pCurrCategory)->m_vpSequences.begin(); pCurrSequence!=(*pCurrCategory)->m_vpSequences.end(); ++pCurrSequence) {
            nFramesTotal += (*pCurrSequence)->GetTotalImageCount();
            mSeqLoads.insert(std::make_pair((*pCurrSequence)->GetExpectedLoad(),(*pCurrSequence)));
        }
    }
    CV_Assert(mSeqLoads.size()==nSeqTotal);
    std::cout << "Parsing complete. [" << vpCategories.size() << " category(ies), "  << nSeqTotal  << " sequence(s)]" << std::endl << std::endl;
    if(nSeqTotal) {
        size_t nSeqProcessed = 1;
        const std::string sCurrResultsPath = g_oDatasetInfo.sResultsPath;
#if WRITE_BGSUB_METRICS_ANALYSIS
        g_oDebugFS = cv::FileStorage(sCurrResultsPath+"_debug.yml",cv::FileStorage::WRITE);
#endif //WRITE_BGSUB_METRICS_ANALYSIS
        PlatformUtils::CreateDirIfNotExist(sCurrResultsPath);
        for(size_t c=0; c<vpCategories.size(); ++c)
            PlatformUtils::CreateDirIfNotExist(sCurrResultsPath+vpCategories[c]->m_sName+"/");
        const time_t nStartupTime = time(nullptr);
        const std::string sStartupTimeStr(asctime(localtime(&nStartupTime)));
        std::cout << "[" << sStartupTimeStr.substr(0,sStartupTimeStr.size()-1) << "]" << std::endl;
#if !HAVE_GPU_SUPPORT && DEFAULT_NB_THREADS>1
        std::cout << "Executing background subtraction with " << ((g_nMaxThreads>nSeqTotal)?nSeqTotal:g_nMaxThreads) << " thread(s)..." << std::endl;
#else //DEFAULT_NB_THREADS==1
        std::cout << "Executing background subtraction..." << std::endl;
#endif //DEFAULT_NB_THREADS==1
#if HAVE_GPU_SUPPORT
        for(auto oSeqIter=mSeqLoads.rbegin(); oSeqIter!=mSeqLoads.rend(); ++oSeqIter) {
            std::cout << "\tProcessing [" << nSeqProcessed << "/" << nSeqTotal << "] (" << oSeqIter->second->m_sParentName << ":" << oSeqIter->second->m_sName << ", L=" << std::scientific << std::setprecision(2) << oSeqIter->first << ")" << std::endl;
            ++nSeqProcessed;
            AnalyzeSequence(0,oSeqIter->second,sCurrResultsPath);
        }
#else //!HAVE_GPU_SUPPORT
        for(auto oSeqIter=mSeqLoads.rbegin(); oSeqIter!=mSeqLoads.rend(); ++oSeqIter) {
            while(g_nActiveThreads>=g_nMaxThreads)
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            std::cout << "\tProcessing [" << nSeqProcessed << "/" << nSeqTotal << "] (" << oSeqIter->second->m_sParentName << ":" << oSeqIter->second->m_sName << ", L=" << std::scientific << std::setprecision(2) << oSeqIter->first << ")" << std::endl;
            ++g_nActiveThreads;
            ++nSeqProcessed;
            std::thread(AnalyzeSequence,nSeqProcessed,oSeqIter->second,sCurrResultsPath).detach();
        }
        while(g_nActiveThreads>0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
#endif //!HAVE_GPU_SUPPORT
        const time_t nShutdownTime = time(nullptr);
        const std::string sShutdownTimeStr(asctime(localtime(&nShutdownTime)));
        std::cout << "[" << sShutdownTimeStr.substr(0,sShutdownTimeStr.size()-1) << "]\n" << std::endl;
#if WRITE_BGSUB_METRICS_ANALYSIS
        std::cout << "Summing and writing metrics results..." << std::endl;
        for(size_t c=0; c<vpCategories.size(); ++c) {
            if(!vpCategories[c]->m_vpSequences.empty()) {
                for(size_t s=0; s<vpCategories[c]->m_vpSequences.size(); ++s)
                    DatasetUtils::Segm::Video::SequenceInfo::WriteMetrics(sCurrResultsPath+vpCategories[c]->m_sName+"/"+vpCategories[c]->m_vpSequences[s]->m_sName+".txt",*vpCategories[c]->m_vpSequences[s]);
                std::sort(vpCategories[c]->m_vpSequences.begin(),vpCategories[c]->m_vpSequences.end(),DatasetUtils::Segm::Video::SequenceInfo::compare<DatasetUtils::Segm::Video::SequenceInfo>);
                DatasetUtils::Segm::Video::CategoryInfo::WriteMetrics(sCurrResultsPath+vpCategories[c]->m_sName+".txt",*vpCategories[c]);
                std::cout << std::endl;
            }
        }
        std::sort(vpCategories.begin(),vpCategories.end(),&DatasetUtils::Segm::Video::CategoryInfo::compare<DatasetUtils::Segm::Video::CategoryInfo>);
        DatasetUtils::Segm::Video::CategoryInfo::WriteMetrics(sCurrResultsPath+"METRICS_TOTAL.txt",vpCategories);
        g_oDebugFS.release();
#endif //WRITE_BGSUB_METRICS_ANALYSIS
        std::cout << "All done." << std::endl;
    }
    else
        std::cout << "No sequences found, all done." << std::endl;
}

int AnalyzeSequence(int nThreadIdx, std::shared_ptr<DatasetUtils::Segm::Video::SequenceInfo> pCurrSequence, const std::string& sCurrResultsPath) {
    srand(0); // for now, assures that two consecutive runs on the same data return the same results
    //srand((unsigned int)time(NULL));
    size_t nCurrFrameIdx = 0;
#if HAVE_GPU_SUPPORT
    size_t nNextFrameIdx = nCurrFrameIdx+1;
    bool bGPUContextInitialized = false;
#endif //HAVE_GPU_SUPPORT
    try {
        CV_Assert(pCurrSequence.get() && pCurrSequence->GetTotalImageCount()>1 && !sCurrResultsPath.empty());
#if DATASET_PRECACHING
        pCurrSequence->StartPrecaching();
#endif //DATASET_PRECACHING
        const std::string sCurrSeqName = pCurrSequence->m_sName.size()>12?pCurrSequence->m_sName.substr(0,12):pCurrSequence->m_sName;
        const size_t nFrameCount = pCurrSequence->GetTotalImageCount();
        const cv::Mat oROI = LIMIT_MODEL_TO_SEQUENCE_ROI?pCurrSequence->GetROI():cv::Mat();
        cv::Mat oCurrInputFrame = pCurrSequence->GetInputFromIndex(nCurrFrameIdx).clone();
        CV_Assert(!oCurrInputFrame.empty());
        CV_Assert(oCurrInputFrame.isContinuous());
#if NEED_GT_MASK
        cv::Mat oCurrGTMask = pCurrSequence->GetGTFromIndex(nCurrFrameIdx).clone();
        CV_Assert(!oCurrGTMask.empty() && oCurrGTMask.isContinuous());
#endif //NEED_GT_MASK
#if HAVE_GPU_SUPPORT
#if DISPLAY_BGSUB_DEBUG_OUTPUT
        cv::Mat oLastInputFrame = oCurrInputFrame.clone();
#endif //DISPLAY_BGSUB_DEBUG_OUTPUT
        cv::Mat oNextInputFrame = pCurrSequence->GetInputFromIndex(nNextFrameIdx);
#if NEED_GT_MASK
#if NEED_LAST_GT_MASK
        cv::Mat oLastGTMask = oCurrGTMask.clone();
#endif // NEED_LAST_GT_MASK
        cv::Mat oNextGTMask = pCurrSequence->GetGTFromIndex(nNextFrameIdx);
#endif //NEED_GT_MASK
#if NEED_FG_MASK
        cv::Mat oLastFGMask(oCurrInputFrame.size(),CV_8UC1,cv::Scalar_<uchar>(0));
#endif //NEED_FG_MASK
#if DISPLAY_BGSUB_DEBUG_OUTPUT
        cv::Mat oLastBGImg;
#endif //DISPLAY_BGSUB_DEBUG_OUTPUT
#else //!HAVE_GPU_SUPPORT
#if (NEED_FG_MASK || !HAVE_GPU_SUPPORT)
        cv::Mat oCurrFGMask(oCurrInputFrame.size(),CV_8UC1,cv::Scalar_<uchar>(0));
#endif //(NEED_FG_MASK || !HAVE_GPU_SUPPORT)
#if DISPLAY_BGSUB_DEBUG_OUTPUT
        cv::Mat oCurrBGImg;
#endif //DISPLAY_BGSUB_DEBUG_OUTPUT
#endif //!HAVE_GPU_SUPPORT
#if HAVE_GLSL
        glAssert(oCurrInputFrame.channels()==1 || oCurrInputFrame.channels()==4);
        cv::Size oWindowSize = oCurrInputFrame.size();
        // note: never construct GL classes before context initialization
        if(glfwInit()==GL_FALSE)
            glError("Failed to init GLFW");
        bGPUContextInitialized = true;
        glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,TARGET_GL_VER_MAJOR);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,TARGET_GL_VER_MINOR);
        glfwWindowHint(GLFW_RESIZABLE,GL_FALSE);
#if !DISPLAY_BGSUB_DEBUG_OUTPUT
        glfwWindowHint(GLFW_VISIBLE,GL_FALSE);
#endif //!DISPLAY_BGSUB_DEBUG_OUTPUT
        std::unique_ptr<GLFWwindow,void(*)(GLFWwindow*)> pWindow(glfwCreateWindow(oWindowSize.width,oWindowSize.height,"changedet_gpu",nullptr,nullptr),glfwDestroyWindow);
        if(!pWindow)
            glError("Failed to create window via GLFW");
        glfwMakeContextCurrent(pWindow.get());
        glewInitErrorCheck;
#endif //HAVE_GLSL
#if USE_LOBSTER_BGSUB
        std::shared_ptr<BackgroundSubtractorLOBSTER> pBGS(new BackgroundSubtractorLOBSTER());
        const double dDefaultLearningRate = BGSLOBSTER_DEFAULT_LEARNING_RATE;
        pBGS->initialize(oCurrInputFrame,oROI);
#elif USE_SUBSENSE_BGSUB
        std::shared_ptr<BackgroundSubtractorSuBSENSE> pBGS(new BackgroundSubtractorSuBSENSE());
        const double dDefaultLearningRate = 0;
        pBGS->initialize(oCurrInputFrame,oROI);
#elif USE_PAWCS_BGSUB
        std::shared_ptr<BackgroundSubtractorPAWCS> pBGS(new BackgroundSubtractorPAWCS());
        const double dDefaultLearningRate = 0;
        pBGS->initialize(oCurrInputFrame,oROI);
#else //USE_VIBE_BGSUB || USE_PBAS_BGSUB
        const size_t m_nInputChannels = (size_t)oCurrInputFrame.channels();
#if USE_VIBE_BGSUB
        std::shared_ptr<cv::BackgroundSubtractorViBe> pBGS;
        if(m_nInputChannels==3)
            pBGS = std::shared_ptr<cv::BackgroundSubtractorViBe>(new BackgroundSubtractorViBe_3ch());
        else
            pBGS = std::shared_ptr<cv::BackgroundSubtractorViBe>(new BackgroundSubtractorViBe_1ch());
        const double dDefaultLearningRate = BGSVIBE_DEFAULT_LEARNING_RATE;
#else //USE_PBAS_BGSUB
        std::shared_ptr<cv::BackgroundSubtractorPBAS> pBGS;
        if(m_nInputChannels==3)
            pBGS = std::shared_ptr<cv::BackgroundSubtractorPBAS>(new BackgroundSubtractorPBAS_3ch());
        else
            pBGS = std::shared_ptr<cv::BackgroundSubtractorPBAS>(new BackgroundSubtractorPBAS_1ch());
        const double dDefaultLearningRate = BGSPBAS_DEFAULT_LEARNING_RATE_OVERRIDE;
#endif //USE_PBAS_BGSUB
        pBGS->initialize(oCurrInputFrame);
#endif //USE_VIBE_BGSUB || USE_PBAS_BGSUB
#if USE_LOBSTER_BGSUB || USE_SUBSENSE_BGSUB || USE_PAWCS_BGSUB
        pBGS->m_sDebugName = pCurrSequence->m_sParentName+"_"+pCurrSequence->m_sName;
#if WRITE_BGSUB_METRICS_ANALYSIS
        pBGS->m_pDebugFS = &g_oDebugFS;
#endif //WRITE_BGSUB_METRICS_ANALYSIS
#if ENABLE_DISPLAY_MOUSE_DEBUG
        pnLatestMouseX = &pBGS->m_nDebugCoordX;
        pnLatestMouseY = &pBGS->m_nDebugCoordY;
#endif //ENABLE_DISPLAY_MOUSE_DEBUG
#endif //USE_LOBSTER_BGSUB || USE_SUBSENSE_BGSUB || USE_PAWCS_BGSUB
#if DISPLAY_BGSUB_DEBUG_OUTPUT
        std::string sDebugDisplayName = pCurrSequence->m_sParentName + std::string(" -- ") + pCurrSequence->m_sName;
        cv::namedWindow(sDebugDisplayName);
#endif //DISPLAY_BGSUB_DEBUG_OUTPUT
#if ENABLE_DISPLAY_MOUSE_DEBUG
        std::string sMouseDebugDisplayName = pCurrSequence->m_sParentName + std::string(" -- ") + pCurrSequence->m_sName + " [MOUSE DEBUG]";
        cv::namedWindow(sMouseDebugDisplayName,0);
        cv::setMouseCallback(sMouseDebugDisplayName,OnMouseEvent,nullptr);
#endif //ENABLE_DISPLAY_MOUSE_DEBUG
#if (WRITE_BGSUB_IMG_OUTPUT || WRITE_BGSUB_AVI_OUTPUT)
        CV_Assert(!sCurrResultsPath.empty());
        PlatformUtils::CreateDirIfNotExist(sCurrResultsPath+pCurrSequence->m_sParentName+"/"+pCurrSequence->m_sName+"/");
#if WRITE_BGSUB_AVI_OUTPUT
        cv::VideoWriter oSegmWriter(sCurrResultsPath+pCurrSequence->m_sParentName+"/"+pCurrSequence->m_sName+"_segm.avi",CV_FOURCC('F','F','V','1'),30,pCurrSequence->GetImageSize(),false);
#endif //WRITE_BGSUB_AVI_OUTPUT
#endif //(WRITE_BGSUB_IMG_OUTPUT || WRITE_BGSUB_AVI_OUTPUT)
#if HAVE_GLSL
        std::shared_ptr<GLImageProcAlgo> pBGS_GPU = std::dynamic_pointer_cast<GLImageProcAlgo>(pBGS);
        if(pBGS_GPU==nullptr)
            glError("BGSub algorithm has no GLImageProcAlgo interface");
        pBGS_GPU->setOutputFetching(NEED_FG_MASK);
#if (GLSL_EVALUATION && WRITE_BGSUB_METRICS_ANALYSIS)
        std::shared_ptr<DatasetUtils::Segm::Video::CDnet::Evaluator> pBGS_GPU_EVAL(new DatasetUtils::Segm::Video::CDnet::Evaluator(pBGS_GPU,nFrameCount));
        pBGS_GPU_EVAL->initialize(oCurrGTMask,oROI.empty()?cv::Mat(oCurrInputFrame.size(),CV_8UC1,cv::Scalar_<uchar>(255)):oROI);
        oWindowSize.width *= pBGS_GPU_EVAL->m_nSxSDisplayCount;
#else //!(GLSL_EVALUATION && WRITE_BGSUB_METRICS_ANALYSIS)
        oWindowSize.width *= pBGS_GPU->m_nSxSDisplayCount;
#endif //!(GLSL_EVALUATION && WRITE_BGSUB_METRICS_ANALYSIS)
        glfwSetWindowSize(pWindow.get(),oWindowSize.width,oWindowSize.height);
        glViewport(0,0,oWindowSize.width,oWindowSize.height);
#endif //HAVE_GLSL
        TIMER_TIC(MainLoop);
#if HAVE_GPU_SUPPORT
        while(nNextFrameIdx<=nFrameCount) {
#else //!HAVE_GPU_SUPPORT
        while(nCurrFrameIdx<nFrameCount) {
#endif //!HAVE_GPU_SUPPORT
            if(!((nCurrFrameIdx+1)%100))
                std::cout << "\t\t" << std::setfill(' ') << std::setw(12) << sCurrSeqName << " @ F:" << std::setfill('0') << std::setw(PlatformUtils::decimal_integer_digit_count((int)nFrameCount)) << nCurrFrameIdx+1 << "/" << nFrameCount << "   [T=" << nThreadIdx << "]" << std::endl;
            const double dCurrLearningRate = (BOOTSTRAP_100_FIRST_FRAMES&&nCurrFrameIdx<=100)?1:dDefaultLearningRate;
            TIMER_INTERNAL_TIC(OverallLoop);
#if HAVE_GPU_SUPPORT
            TIMER_INTERNAL_TIC(PipelineUpdate);
            pBGS->apply(oNextInputFrame,dCurrLearningRate);
            TIMER_INTERNAL_TOC(PipelineUpdate);
#if (GLSL_EVALUATION && WRITE_BGSUB_METRICS_ANALYSIS)
            pBGS_GPU_EVAL->apply(oNextGTMask);
#endif //(GLSL_EVALUATION && WRITE_BGSUB_METRICS_ANALYSIS)
            TIMER_INTERNAL_TIC(VideoQuery);
#if DISPLAY_BGSUB_DEBUG_OUTPUT
            oCurrInputFrame.copyTo(oLastInputFrame);
            const cv::Mat& oInputFrame = oLastInputFrame;
            oNextInputFrame.copyTo(oCurrInputFrame);
#endif //DISPLAY_BGSUB_DEBUG_OUTPUT
            if(++nNextFrameIdx<nFrameCount)
                oNextInputFrame = pCurrSequence->GetInputFromIndex(nNextFrameIdx);
#if ENABLE_DISPLAY_MOUSE_DEBUG
            cv::imshow(sMouseDebugDisplayName,oNextInputFrame);
#endif //ENABLE_DISPLAY_MOUSE_DEBUG
#if NEED_GT_MASK
#if NEED_LAST_GT_MASK
            oCurrGTMask.copyTo(oLastGTMask);
            const cv::Mat& oGTMask = oLastGTMask;
            oNextGTMask.copyTo(oCurrGTMask);
#endif //NEED_LAST_GT_MASK
            if(nNextFrameIdx<nFrameCount)
                oNextGTMask = pCurrSequence->GetGTFromIndex(nNextFrameIdx);
#endif //NEED_GT_MASK
            TIMER_INTERNAL_TOC(VideoQuery);
#if HAVE_GLSL
            glErrorCheck;
            if(glfwWindowShouldClose(pWindow.get()))
                break;
            glfwPollEvents();
#if DISPLAY_BGSUB_DEBUG_OUTPUT
            if(glfwGetKey(pWindow.get(),GLFW_KEY_ESCAPE) || glfwGetKey(pWindow.get(),GLFW_KEY_Q))
                break;
            glfwSwapBuffers(pWindow.get());
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
#endif //DISPLAY_BGSUB_DEBUG_OUTPUT
#endif //HAVE_GLSL
#if NEED_FG_MASK
            pBGS->getLatestForegroundMask(oLastFGMask);
            //cv::medianBlur(oLastFGMask,oLastFGMask,9);
            if(!oROI.empty())
                cv::bitwise_or(oLastFGMask,UCHAR_MAX/2,oLastFGMask,oROI==0);
            //cv::imshow("oLastFGMask",oLastFGMask);
            //cv::waitKey(1);
            const cv::Mat& oFGMask = oLastFGMask;
#endif //NEED_FG_MASK
#if DISPLAY_BGSUB_DEBUG_OUTPUT
            pBGS->getBackgroundImage(oLastBGImg);
            if(!oROI.empty())
                cv::bitwise_or(oLastBGImg,UCHAR_MAX/2,oLastBGImg,oROI==0);
            const cv::Mat& oBGImg = oLastBGImg;
#endif //DISPLAY_BGSUB_DEBUG_OUTPUT
#else //!HAVE_GPU_SUPPORT
            TIMER_INTERNAL_TIC(VideoQuery);
            oCurrInputFrame = pCurrSequence->GetInputFromIndex(nCurrFrameIdx);
#if DISPLAY_BGSUB_DEBUG_OUTPUT
            const cv::Mat& oInputFrame = oCurrInputFrame;
#endif //DISPLAY_BGSUB_DEBUG_OUTPUT
#if ENABLE_DISPLAY_MOUSE_DEBUG
            cv::imshow(sMouseDebugDisplayName,oCurrInputFrame);
#endif //ENABLE_DISPLAY_MOUSE_DEBUG
#if NEED_GT_MASK
            oCurrGTMask = pCurrSequence->GetGTFromIndex(nCurrFrameIdx);
            const cv::Mat oGTMask = oCurrGTMask;
#endif //NEED_GT_MASK
            TIMER_INTERNAL_TOC(VideoQuery);
            TIMER_INTERNAL_TIC(PipelineUpdate);
            pBGS->apply(oCurrInputFrame,oCurrFGMask,dCurrLearningRate);
            TIMER_INTERNAL_TOC(PipelineUpdate);
            if(!oROI.empty())
                cv::bitwise_or(oCurrFGMask,UCHAR_MAX/2,oCurrFGMask,oROI==0);
#if NEED_FG_MASK
            const cv::Mat& oFGMask = oCurrFGMask;
#endif //NEED_FG_MASK
#if DISPLAY_BGSUB_DEBUG_OUTPUT
            pBGS->getBackgroundImage(oCurrBGImg);
            if(!oROI.empty())
                cv::bitwise_or(oCurrBGImg,UCHAR_MAX/2,oCurrBGImg,oROI==0);
            const cv::Mat& oBGImg = oCurrBGImg;
#endif //DISPLAY_BGSUB_DEBUG_OUTPUT
#endif //!HAVE_GPU_SUPPORT
#if DISPLAY_BGSUB_DEBUG_OUTPUT
#if ENABLE_DISPLAY_MOUSE_DEBUG
            cv::Mat oDebugDisplayFrame = DatasetUtils::Segm::Video::CDnet::GetDebugDisplayFrame(oInputFrame,oBGImg,oFGMask,oGTMask,oROI,nCurrFrameIdx,(pnLatestMouseX&&pnLatestMouseY)?cv::Point(*pnLatestMouseX,*pnLatestMouseY):cv::Point(-1,-1));
#else //!ENABLE_DISPLAY_MOUSE_DEBUG
            cv::Mat oDebugDisplayFrame = DatasetUtils::Segm::Video::CDnet::GetDebugDisplayFrame(oInputFrame,oBGImg,oFGMask,oGTMask,oROI,nCurrFrameIdx);
#endif //!ENABLE_DISPLAY_MOUSE_DEBUG
            cv::Mat oDebugDisplayFrameResized;
            if(oDebugDisplayFrame.cols>1280 || oDebugDisplayFrame.rows>960)
                cv::resize(oDebugDisplayFrame,oDebugDisplayFrameResized,cv::Size(oDebugDisplayFrame.cols/2,oDebugDisplayFrame.rows/2));
            else
                oDebugDisplayFrameResized = oDebugDisplayFrame;
            cv::imshow(sDebugDisplayName,oDebugDisplayFrameResized);
            int nKeyPressed;
            if(g_bContinuousUpdates)
                nKeyPressed = cv::waitKey(1);
            else
                nKeyPressed = cv::waitKey(0);
            if(nKeyPressed!=-1) {
                nKeyPressed %= (UCHAR_MAX+1); // fixes return val bug in some opencv versions
                std::cout << "nKeyPressed = " << nKeyPressed%(UCHAR_MAX+1) << std::endl;
            }
            if(nKeyPressed==' ')
                g_bContinuousUpdates = !g_bContinuousUpdates;
            else if(nKeyPressed==(int)'q')
                break;
#endif //DISPLAY_BGSUB_DEBUG_OUTPUT
#if WRITE_BGSUB_AVI_OUTPUT
            oSegmWriter.write(oFGMask);
#endif //WRITE_BGSUB_AVI_OUTPUT
#if WRITE_BGSUB_IMG_OUTPUT
            DatasetUtils::Segm::Video::WriteResult(sCurrResultsPath,pCurrSequence->m_sParentName,pCurrSequence->m_sName,g_oDatasetInfo.sResultPrefix,nCurrFrameIdx+g_oDatasetInfo.nResultIdxOffset,g_oDatasetInfo.sResultSuffix,oFGMask,g_vnResultsComprParams);
#endif //WRITE_BGSUB_IMG_OUTPUT
#if (WRITE_BGSUB_METRICS_ANALYSIS && (!GLSL_EVALUATION || VALIDATE_GLSL_EVALUATION))
            DatasetUtils::Segm::Video::CDnet::AccumulateMetricsFromResult(oFGMask,oGTMask,oROI,pCurrSequence->m_oMetrics);
#endif //(WRITE_BGSUB_METRICS_ANALYSIS && (!GLSL_EVALUATION || VALIDATE_GLSL_EVALUATION))
            TIMER_INTERNAL_TOC(OverallLoop);
#if ENABLE_INTERNAL_TIMERS
            std::cout << "VideoQuery=" << TIMER_INTERNAL_ELAPSED_MS(VideoQuery) << "ms,  "
                      << "PipelineUpdate=" << TIMER_INTERNAL_ELAPSED_MS(PipelineUpdate) << "ms,  "
                      << "OverallLoop=" << TIMER_INTERNAL_ELAPSED_MS(OverallLoop) << "ms" << std::endl;
#endif //ENABLE_INTERNAL_TIMERS
            ++nCurrFrameIdx;
        }
        TIMER_TOC(MainLoop);
        const double dTimeElapsed = TIMER_ELAPSED_MS(MainLoop)/1000;
        const double dAvgFPS = (double)nFrameCount/dTimeElapsed;
        std::cout << "\t\t" << std::setfill(' ') << std::setw(12) << sCurrSeqName << " @ end, " << int(dTimeElapsed) << " sec in-thread (" << (int)floor(dAvgFPS+0.5) << " FPS)" << std::endl;
#if WRITE_BGSUB_METRICS_ANALYSIS
#if GLSL_EVALUATION
#if VALIDATE_GLSL_EVALUATION
        printf("cpu eval:\n\tnTP=%" PRIu64 ", nTN=%" PRIu64 ", nFP=%" PRIu64 ", nFN=%" PRIu64 ", nSE=%" PRIu64 ", tot=%" PRIu64 "\n",pCurrSequence->nTP,pCurrSequence->nTN,pCurrSequence->nFP,pCurrSequence->nFN,pCurrSequence->nSE,pCurrSequence->nTP+pCurrSequence->nTN+pCurrSequence->nFP+pCurrSequence->nFN);
#endif //VALIDATE_GLSL_EVALUATION
        pBGS_GPU_EVAL->getCumulativeCounts(pCurrSequence->m_oMetrics);
#if VALIDATE_GLSL_EVALUATION
        printf("gpu eval:\n\tnTP=%" PRIu64 ", nTN=%" PRIu64 ", nFP=%" PRIu64 ", nFN=%" PRIu64 ", nSE=%" PRIu64 ", tot=%" PRIu64 "\n",pCurrSequence->nTP,pCurrSequence->nTN,pCurrSequence->nFP,pCurrSequence->nFN,pCurrSequence->nSE,pCurrSequence->nTP+pCurrSequence->nTN+pCurrSequence->nFP+pCurrSequence->nFN);
#endif //VALIDATE_GLSL_EVALUATION
#endif //GLSL_EVALUATION
        pCurrSequence->m_oMetrics.dTimeElapsed_sec = dTimeElapsed;
        DatasetUtils::Segm::Video::SequenceInfo::WriteMetrics(sCurrResultsPath+pCurrSequence->m_sParentName+"/"+pCurrSequence->m_sName+".txt",*pCurrSequence);
#endif //WRITE_BGSUB_METRICS_ANALYSIS
#if DISPLAY_BGSUB_DEBUG_OUTPUT
        cv::destroyWindow(sDebugDisplayName);
#endif //DISPLAY_BGSUB_DEBUG_OUTPUT
#if ENABLE_DISPLAY_MOUSE_DEBUG
        cv::setMouseCallback(sMouseDebugDisplayName,OnMouseEvent,nullptr);
        pnLatestMouseX = nullptr;
        pnLatestMouseY = nullptr;
        cv::destroyWindow(sMouseDebugDisplayName);
#endif //ENABLE_DISPLAY_MOUSE_DEBUG
    }
#if HAVE_GLSL
    catch(const GLException& e) {std::cout << "\n!!!!!!!!!!!!!!\nTop level caught GLException:\n" << e.what() << "\n!!!!!!!!!!!!!!\n" << std::endl;}
#endif //HAVE_GLSL
    catch(const cv::Exception& e) {std::cout << "\n!!!!!!!!!!!!!!\nTop level caught cv::Exception:\n" << e.what() << "\n!!!!!!!!!!!!!!\n" << std::endl;}
    catch(const std::runtime_error& e) {std::cout << "\n!!!!!!!!!!!!!!\nTop level caught std::runtime_error:\n" << e.what() << "\n!!!!!!!!!!!!!!\n" << std::endl;}
    catch(...) {std::cout << "\n!!!!!!!!!!!!!!\nTop level caught unhandled exception\n!!!!!!!!!!!!!!\n" << std::endl;}
#if HAVE_GPU_SUPPORT
    if(bGPUContextInitialized) {
#if HAVE_GLSL
        glfwTerminate();
#endif //HAVE_GLSL
    }
#else //!HAVE_GPU_SUPPORT
    g_nActiveThreads--;
#endif //!HAVE_GPU_SUPPORT
    return 0;
}
