/* ----------------------------------------------------------------------------
 * Copyright 2017, Massachusetts Institute of Technology,
 * Cambridge, MA 02139
 * All Rights Reserved
 * Authors: Luca Carlone, et al. (see THANKS for the full author list)
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file   testFrame.h
 * @brief  test Frame
 * @author Luca Carlone
 */

#include <CppUnitLite/TestHarness.h>
#include "Frame.h"
#include "test_config.h"

using namespace gtsam;
using namespace std;
using namespace VIO;
using namespace cv;

static const string chessboardImgName = string(DATASET_PATH) + "/chessboard.png";
static const string whitewallImgName = string(DATASET_PATH) + "/whitewall.png";
static const string sensorPath = string(DATASET_PATH) + "/sensor.yaml";
static const int imgWidth = 752;
static const int imgHeight = 480;
static const double tol = 1e-7;

/* ************************************************************************* */
TEST(testFrame, constructor) {
  // Construct a frame from image name.
  FrameId id = 0;
  Timestamp tmp = 123;
  const string imgName = string(DATASET_PATH) + "/chessboard.png";
  Frame f(id, tmp, imgName, CameraParams());
  EXPECT(f.id_ == id);
  EXPECT(f.timestamp_ == tmp);
  // check image:
  Mat img = imread(imgName, IMREAD_ANYCOLOR);
  EXPECT(UtilsOpenCV::CvMatCmp(f.img_, img));
  EXPECT(!f.isKeyframe_); // false by default
  EXPECT(CameraParams().equals( f.cam_param_ ));
}

/* ************************************************************************* */
TEST(testFrame, ExtractCornersChessboard) {
  Frame f(0, 0, chessboardImgName, CameraParams());
  f.ExtractCorners();
  int numCorners_expected = 7 * 9;
  int numCorners_actual = f.keypoints_.size();
  // Assert that there are right number of corners!
  EXPECT(numCorners_actual == numCorners_expected);
}

/* ************************************************************************* */
TEST(testFrame, ExtractCornersWhiteBoard) {
  Frame f(0, 0, whitewallImgName, CameraParams());
  f.ExtractCorners();
  int numCorners_expected = 0;
  int numCorners_actual = f.keypoints_.size();
  // Assert that there are no corners!
  EXPECT(numCorners_actual == numCorners_expected);
}

/* ------------------------------------------------------------------------- */
TEST(testFrame, getNrValidKeypoints) {
  Frame f(0, 0, chessboardImgName, CameraParams());
  const int nrValidExpected = 200;
  const int outlier_rate = 5; // Insert one outlier every 5 valid landmark ids.
  for (int i = 0; i < nrValidExpected; i++) {
    if (i % outlier_rate == 0) {
      f.landmarks_.push_back(-1);
    }
    f.landmarks_.push_back(i); // always push a valid and sometimes also an outlier
  }
  int nrValidActual = f.getNrValidKeypoints();
  EXPECT(nrValidActual == nrValidExpected);
}

/* ************************************************************************* */
TEST(testFrame, CalibratePixel) {
  // Perform a scan on the grid to verify the correctness of pixel calibration!
  const int numTestRows = 8;
  const int numTestCols = 8;

  // Get the camera parameters
  CameraParams camParams;
  camParams.parseYAML(sensorPath);

  // Generate the pixels
  KeypointsCV testPointsCV;
  testPointsCV.reserve(numTestRows * numTestCols);

  for (int r = 0; r < numTestRows; r++) {
    for (int c = 0; c < numTestCols; c++) {
      testPointsCV.push_back(KeypointCV(c * imgWidth / (numTestCols - 1),
          r * imgHeight / (numTestRows - 1)));
    }
  }
  // Calibrate, and uncalibrate the point, verify that we get the same point
  for (KeypointsCV::iterator iter = testPointsCV.begin(); iter != testPointsCV.end(); iter++) {
    Vector3 versor = Frame::CalibratePixel(*iter, camParams);
    EXPECT_DOUBLES_EQUAL(versor.norm(), 1, tol);

    // distort the pixel again
    versor = versor / versor(2);
    Point2 uncalibrated_px_actual = camParams.calibration_.uncalibrate(Point2(versor(0), versor(1)));
    Point2 uncalibrated_px_expected = Point2(iter->x, iter->y);
    Point2 px_mismatch = uncalibrated_px_actual - uncalibrated_px_expected;
    EXPECT(px_mismatch.vector().norm() < 0.5);
  }
}

/* ************************************************************************* */
int main() { TestResult tr; return TestRegistry::runAllTests(tr); }
/* ************************************************************************* */
