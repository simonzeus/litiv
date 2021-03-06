
#include "litiv/datasets.hpp"
#include "litiv/imgproc.hpp"
#include "litiv/test.hpp"

TEST(datasets_notarray,regression_custom) {
    lv::datasets::setParserVerbosity(0);
    using DatasetType = lv::Dataset_<lv::DatasetTask_EdgDet,lv::Dataset_Custom,lv::NonParallel>;
    const std::string sDatasetName = "customtest";
    const std::string sOutputRootPath = TEST_OUTPUT_DATA_ROOT "/custom_dataset_test/";
    const std::vector<std::string> vsWorkBatchDirs = {"batch1","batch2","batch3"};
    const std::string sOutputPrefix = "edge_mask_";
    const std::string sOutputSuffix = ".png";
    DatasetType::Ptr pDataset = DatasetType::create(
        sDatasetName,
        lv::addDirSlashIfMissing(SAMPLES_DATA_ROOT)+"custom_dataset_ex/",
        sOutputRootPath,
        sOutputPrefix,
        sOutputSuffix,
        vsWorkBatchDirs,
        std::vector<std::string>(),
        std::vector<std::string>(),
        true,
        false,
        false,
        1.0
    );
    ASSERT_TRUE(pDataset.get()!=nullptr);
    pDataset->startPrecaching();
    ASSERT_EQ(pDataset->getName(),sDatasetName);
    ASSERT_TRUE(lv::checkIfExists(sOutputRootPath));
    lv::IDataHandlerPtrArray vpBatches = pDataset->getBatches(false);
    ASSERT_EQ(vpBatches.size(),size_t(3));
    ASSERT_EQ(pDataset->getInputCount(),size_t(6));
    ASSERT_EQ(pDataset->getGTCount(),size_t(0));
    ASSERT_FALSE(pDataset->isBare());
    ASSERT_TRUE(pDataset->isGroup());
    ASSERT_EQ(pDataset->getOutputNamePrefix(),sOutputPrefix);
    ASSERT_EQ(pDataset->getOutputNameSuffix(),sOutputSuffix);
    EXPECT_EQ(pDataset->getWorkBatchDirs(),vsWorkBatchDirs);
    EXPECT_EQ(pDataset->getSkippedDirTokens(),std::vector<std::string>());
    EXPECT_EQ(pDataset->getGrayscaleDirTokens(),std::vector<std::string>());
    EXPECT_DOUBLE_EQ(pDataset->getScaleFactor(),1.0);
    EXPECT_EQ(pDataset->getParent(),lv::IDataHandlerConstPtr());
    EXPECT_TRUE(pDataset->isRoot());
    EXPECT_EQ(pDataset->getRoot(),pDataset);
    EXPECT_FALSE(pDataset->isGrayscale());
    EXPECT_GE(pDataset->getExpectedLoad(),0.0);
    std::shared_ptr<IEdgeDetector> pAlgo = std::make_shared<EdgeDetectorLBSP>();
    size_t nBatchIdx = 0;
    for(auto pBatchIter = vpBatches.begin(); pBatchIter!=vpBatches.end(); ++pBatchIter) {
        DatasetType::WorkBatch& oBatch = dynamic_cast<DatasetType::WorkBatch&>(**pBatchIter);
        ASSERT_TRUE(lv::checkIfExists(sOutputRootPath+oBatch.getName()));
        ASSERT_FALSE(oBatch.isBare());
        ASSERT_FALSE(oBatch.isGroup());
        ASSERT_EQ(oBatch.getOutputNamePrefix(),sOutputPrefix);
        ASSERT_EQ(oBatch.getOutputNameSuffix(),sOutputSuffix);
        EXPECT_DOUBLE_EQ(oBatch.getScaleFactor(),1.0);
        EXPECT_FALSE(oBatch.isRoot());
        EXPECT_EQ(oBatch.getRoot(),pDataset);
        EXPECT_FALSE(oBatch.isGrayscale());
        EXPECT_GE(oBatch.getExpectedLoad(),0.0);
        ASSERT_EQ(oBatch.getImageCount(),oBatch.getInputCount());
        EXPECT_EQ(oBatch.getBatches(false),lv::IDataHandlerPtrArray());
        ASSERT_EQ(oBatch.getDatasetTask(),lv::DatasetTask_EdgDet);
        ASSERT_EQ(oBatch.getDatasetSource(),lv::DatasetSource_Image);
        ASSERT_EQ(oBatch.getDataset(),lv::Dataset_Custom);
        const size_t nTotPackets = oBatch.getImageCount();
        const size_t nExpectedTotPackets = size_t(nBatchIdx==0?3:nBatchIdx==1?1:2);
        ASSERT_EQ(nTotPackets,nExpectedTotPackets);
        ASSERT_EQ(oBatch.getGTCount(),size_t(0));
        size_t nProcessedPackets = 0;
        oBatch.startProcessing();
        while(nProcessedPackets<nTotPackets) {
            const cv::Mat& oImage = oBatch.getInput(nProcessedPackets);
            ASSERT_TRUE(!oImage.empty());
            cv::Mat oEdgeMask;
            pAlgo->apply(oImage,oEdgeMask);
            oBatch.push(oEdgeMask,nProcessedPackets);
            std::stringstream sstr;
            sstr << sOutputRootPath << "/" << oBatch.getName() << "/" << sOutputPrefix << oBatch.getOutputName(nProcessedPackets++) << sOutputSuffix;
            ASSERT_TRUE(lv::checkIfExists(sstr.str()));
            cv::Mat oOut = cv::imread(sstr.str(),cv::IMREAD_GRAYSCALE);
            ASSERT_TRUE(cv::isEqual<uint8_t>(oEdgeMask,oOut));
        }
        oBatch.stopProcessing();
        EXPECT_GT(oBatch.getFinalProcessTime(),0.0);
        EXPECT_EQ(oBatch.getFinalOutputCount(),oBatch.getInputCount());
        ++nBatchIdx;
    }
    pDataset->writeEvalReport();
    ASSERT_TRUE(lv::checkIfExists(sOutputRootPath+"/customtest.txt"));
}

TEST(datasets_notarray,regression_specialization) {
    // ... @@@@ TODO
}