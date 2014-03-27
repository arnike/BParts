#include "data_io.h"
#include "segmentation.h"
#include "mesher.h"
#include "constants.h"
#include "keyframe.h"
#include "classifier_svm.h"
#include "timer.h"
#include "opflow.h"
#include "opflow_tensor.h"
#include "ldof.h"
#include "CFilter.h"

// TODO: REMOVE
#include <pcl/visualization/cloud_viewer.h>

#define BENCHMARK 1
#define DEBUG 1

#ifdef BENCHMARK
timer::Timer t;
#endif

void mark_body_part(cv::Mat& image, cv::Point centre, classifier::BodyPart body_part) {

	cv::Scalar colour;
	bool negative = false;

	switch(body_part) {
	case classifier::HEAD:
		colour = cv::Scalar(0, 0, 255);
		break;
	case classifier::LEFT_HAND:
	case classifier::RIGHT_HAND:
		colour = cv::Scalar(0, 255, 0);
		break;
	case classifier::LEFT_FOOT:
	case classifier::RIGHT_FOOT:
		colour = cv::Scalar(255, 0, 0);
		break;
	case classifier::ZERO:
		negative = true;
		break;
	default:
		negative = true;
		break;
	}

	if (!negative) {
		cv::circle(image,
					centre,
					 3.0,
					 colour,
					 -1,
					 8);
	}
}

float l2norm(cv::Point a, cv::Point b) {
	return sqrt((a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y - b.y));
}
float precision(float tp, float fp) {
	return tp/(tp + fp);
}

float recall(float tp, float fn) {
	return tp/(tp + fn);
}

int main_(int argc, char * argv[]) {
	io::DataIO data(io::Bonn);

	cv::Mat depth_image;
	/********** Reading in new data ***********/
	//while(data.read(depth_image));
	/******************************************/

	data.extract_data();

	return 1;
}

int main(int argc, char * argv[]) {

	const int start_from_frame = 0;
	const float distance_threshold = 15.f;
	const int num_attr = 1681;
	const int K = pose::kNumInterestPoints;

	int head_fp = 0;
	int head_tp = 0;
	int head_fn = 0;

	int hand_fp = 0;
	int hand_tp = 0;
	int hand_fn = 0;

	int left_hand_fp = 0;
	int left_hand_tp = 0;
	int left_hand_fn = 0;

	int right_hand_fp = 0;
	int right_hand_tp = 0;
	int right_hand_fn = 0;

	io::DataIO data(io::kCurrentDataset);
	classifier::ClassifierSVM svm(num_attr);
	svm.read_model("/media/neek/DATA/MAINF2307/data/Stanford_training/head_hands/data.scale.model");

	opflow::OFTensor<float> aImage1, aImage2;
	opflow::OFTensor<float> aForward, aBackward;

	/******* DATA EXTRACTION *******/
	//data.extract_data();

	cv::Mat depth_image, depth_image_prev, rgb_image;
//	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>());
//	pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_rgb(new pcl::PointCloud<pcl::PointXYZRGB>());

	int frame_idx = 0;
	while(true) {

		/********** Reading in new data ***********/
#ifdef BENCHMARK
t.start();
#endif
		bool is_successful = data.read(depth_image);
#ifdef BENCHMARK
t.stop();
std::cout << "(BENCHMARKING) Reading data: " << t.duration() << "ms" << std::endl;
#endif
		/******************************************/
		if (!is_successful) break;

		if (frame_idx < start_from_frame) {
			frame_idx++;
			continue;
		}

		bool head_detected = false;
		bool left_hand_detected = false;
		bool right_hand_detected = false;

		// For evaluation purposes
		cv::Point centre_truth_head, centre_truth_left_hand, centre_truth_right_hand,
										centre_truth_left_foot, centre_truth_right_foot;

		data.get_ground_truth(centre_truth_head, centre_truth_left_hand, centre_truth_right_hand,
										centre_truth_left_foot, centre_truth_right_foot);

		// Converting the image for display purposes
		cv::Mat depth_image_flow, depth_image_display;
		cv::normalize(depth_image, depth_image_flow, 0.0, 255.0, cv::NORM_MINMAX);
		depth_image_flow.convertTo(depth_image_display, CV_8U);
		cv::cvtColor(depth_image_display, depth_image_display, CV_GRAY2RGB);

		//pcl::PointCloud<pcl::PointXYZ>::Ptr result(new pcl::PointCloud<pcl::PointXYZ>());

		/******** Downsampling of the cloud ********/
//#ifdef BENCHMARK
//t.start();
//#endif
//		mesher::downsample(cloud, result, pose::kDownsampleCellSize);
//#ifdef BENCHMARK
//t.stop();
//std::cout << "(BENCHMARKING) Downsampling of the cloud: " << t.duration() << "ms" <<  std::endl;
//#endif
		/******************************************/

		/************** Optical flow **************/
		cv::Mat image_of(depth_image.size(), CV_32F, 0.f);

		if (opflow::ON) {
			aImage2.readFromMat(depth_image_flow);

			NFilter::recursiveSmoothX(aImage2,0.8f);
			NFilter::recursiveSmoothY(aImage2,0.8f);

			if (frame_idx > start_from_frame) {
#ifdef BENCHMARK
t.start();
#endif
ldof(aImage1, aImage2, aForward, aBackward);
#ifdef BENCHMARK
t.stop();
std::cout << "(BENCHMARKING) Optical Flow: " << t.duration() << "ms" << std::endl;
#endif
			cv::Mat image_f(cv::Size(aForward.xSize(), aForward.ySize()), CV_32F);
			cv::Mat image_b(cv::Size(aBackward.xSize(), aBackward.ySize()), CV_32F);

			aForward.writeToMat(image_f);
			aBackward.writeToMat(image_b);
			opflow::merge_flows(image_f, image_b, image_of);
			}

			aImage1 = aImage2;

			cv::imshow("OF", image_of);
			//cv::waitKey(0);
		}
		/******************************************/


		/************* Creating a mesh ************/
#ifdef BENCHMARK
t.start();
#endif
		mesher::Mesh mesh(pose::kMeshDistanceThreshold, K);
		mesh.compute_with_flow(depth_image, image_of, 200.0);
		//mesh.set_pointcloud(result);
		//mesh.set_zfilter(40.f, 60.f);
		//mesh.compute();

#ifdef BENCHMARK
t.stop();
std::cout << "(BENCHMARKING) Creating a mesh: " << t.duration() << "ms" << std::endl;
#endif
		/******************************************/

		cv::Mat segments;
		depth_image_display.convertTo(segments, CV_8UC3);
		mesh.colour_mat(segments);
//		pcl::PointCloud<pcl::PointXYZRGB>::Ptr colour_pcl(new pcl::PointCloud<pcl::PointXYZRGB>());
//		mesh.colour_cloud(colour_pcl);
//
//		std::cout << "Mesh size: " << mesh.size() << std::endl;
//		std::cout << "Size of the new cloud: " << colour_pcl->size() << std::endl;

		// To remove
		//pcl::visualization::CloudViewer viewer("Simple Cloud Viewer");
		//viewer.showCloud(colour_pcl, "God knows what");
		//while (!viewer.wasStopped()) {}

		std::vector<cv::Point> keypoints, key_orientations;
		mesh.get_interest_points(keypoints, key_orientations);

		// for prediction step
		depth_image.convertTo(depth_image, CV_32F);
		cv::normalize(depth_image, depth_image, -1.0, 1.0, cv::NORM_MINMAX);
		mesh.mark_centroids(depth_image_display, 1.0);
//
		keyframe::Keyframe kframe(depth_image);
//		cv::imshow("Depth image", depth_image);
//		cv::waitKey(0);

		for (int i = 0; i < keypoints.size(); i++) {
			cv::Vec4i orientation;
			cv::Point centre;
			cv::Mat patch;

			//std::cerr << "Index: " << i << std::endl;

			centre.x = keypoints[i].x;
			centre.y = keypoints[i].y;
			orientation[0] = key_orientations[i].x;
			orientation[1] = key_orientations[i].y;
			orientation[2] = keypoints[i].x;
			orientation[3] = keypoints[i].y;

			//mark_body_part(segments, cv::Point(key_orientations[i].x, key_orientations[i].y), classifier::HEAD);
			mark_body_part(segments, centre, classifier::RIGHT_FOOT);

			if (!kframe.extract_part(patch, centre, orientation, pose::kDescriptorSize)) {
				//std::cerr << "Skipping" << std::endl;
				continue;
			}

			cv::Mat part_copy;
			patch.copyTo(part_copy);
			cv::normalize(part_copy, part_copy, 0.0, 255.0, cv::NORM_MINMAX);
			part_copy.convertTo(part_copy, CV_8U);

//			cv::imshow("Interest Point", part_copy);
//			cv::waitKey(0);

			//std::cerr << "Prediction: " << svm.predict(part) << std::endl;
			float confidence;
			classifier::BodyPart body_part;

			/************* SVM prediction *************/
#ifdef BENCHMARK
t.start();
#endif
			svm.predict_probability(patch, &body_part, &confidence);
#ifdef BENCHMARK
t.stop();
std::cout << "(BENCHMARKING) SVM prediction: " << t.duration() << "ms" << std::endl;
#endif
			/******************************************/
			std::cout << "CONF = " << confidence << std::endl;
			std::cout << "BP = " << body_part << std::endl;
			cv::imshow("Interest Point", part_copy);
			cv::waitKey(0);


			if (confidence > classifier::kConfidenceThreshold) {
				mark_body_part(depth_image_display, centre, body_part);



			switch(body_part) {
			case classifier::HEAD:
			{
				if (centre_truth_head.x > 0) {
					float dist = l2norm(centre, centre_truth_head);
					if (dist < distance_threshold && !head_detected) {
						head_tp += 1;
						//std::cerr << "Head detected at: " << dist << std::endl;
						head_detected = true;
					} else {
						head_fp++;
					}

				}
			}
			break;
			case classifier::LEFT_HAND:
			case classifier::RIGHT_HAND:
			{
				if (centre_truth_left_hand.x > 0 && centre_truth_right_hand.x > 0) {
					//mark_body_part(depth_image_display, centre_truth_left_hand, classifier::LEFT_FOOT);
					//mark_body_part(depth_image_display, centre_truth_right_hand, classifier::LEFT_FOOT);
					float dist = std::min(l2norm(centre, centre_truth_left_hand),
											l2norm(centre, centre_truth_right_hand));
					if (dist < distance_threshold && !left_hand_detected) {
						hand_tp++;
						left_hand_detected = true;
					} else if (dist < distance_threshold && !right_hand_detected) {
						hand_tp++;
						right_hand_detected = true;
					} else {
						hand_fp++;
					}

					//std::cerr << "Hand detected at: " << dist << std::endl;
				}
			}
			break;
			default:
				break;
			}

			}


			/* TODO: Debug
			cv::circle(depth_image,
						centre,
					 1.0,
					 cv::Scalar( 255, 255, 255 ),
					 -1,
					 8);

			cv::circle(depth_image,
						cv::Point(key_orientations[i].x, key_orientations[i].y),
					 1.0,
					 cv::Scalar( 0, 0, 0 ),
					 -1,
					 8);

			cv::imshow("Depth image", depth_image);
			cv::waitKey(0);*/

		}

		if (centre_truth_head.x > 0 && !head_detected) head_fn++;
		if (centre_truth_left_hand.x > 0 && !left_hand_detected) hand_fn++;
		if (centre_truth_right_hand.x > 0 && !right_hand_detected) hand_fn++;

		cv::imshow("Segments", segments);
		cv::imshow("Body parts", depth_image_display);
		cv::imwrite("body_part.png", depth_image_display);
		cv::imwrite("segments.png", segments);

		cv::normalize(image_of, image_of, 0, 128, cv::NORM_MINMAX);
		image_of.convertTo(image_of, CV_8UC1);
		cv::imwrite("of.png", image_of);
		cv::waitKey(0);

		//cloud = boost::make_shared<pcl::PointCloud<pcl::PointXYZ> >();

		depth_image_prev = depth_image;
		frame_idx++;
		//break;
	}

	std::cout << "Head Precision: " << precision(head_tp, head_fp) << std::endl;
	std::cout << "Hand Precision: " << precision(hand_tp, hand_fp) << std::endl;

	std::cout << "Head Recall: " << recall(head_tp, head_fn) << std::endl;
	std::cout << "Hand Recall: " << recall(hand_tp, hand_fn) << std::endl;

	return 0;
}

int main_flow(int argc, char * argv[]) {
//	const int mStep = 1;
//	const int mSequenceLength = 1;
//	CMatrix<float> colour_code;
//	opflow::buildColorCode(colour_code);
//
//	std::vector<CTrack> mTracks;

	io::DataIO data(io::StanfordEval);
	cv::Mat depth_image;
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>());
	data.read(depth_image, cloud);

//	int tt = 0;
//
//	int mXSize = depth_image.cols;
//	int mYSize = depth_image.rows;
//
//	int aSize = mXSize*mYSize;
//	CVector<float> color = CVector<float>(3);

	cv::imshow("Image one", depth_image);
	cv::normalize(depth_image, depth_image, 0.0, 255.0, cv::NORM_MINMAX);
	//depth_image.convertTo(depth_image, CV_8U);

	opflow::OFTensor<float> aImage1, aImage2;
	aImage1.readFromMat(depth_image);

	NFilter::recursiveSmoothX(aImage1,0.8f);
	NFilter::recursiveSmoothY(aImage1,0.8f);

	while (data.read(depth_image, cloud)) {


		cv::imshow("Image two", depth_image);
		cv::normalize(depth_image, depth_image, 0.0, 255.0, cv::NORM_MINMAX);
		aImage2.readFromMat(depth_image);

		opflow::OFTensor<float> aForward, aBackward;

		NFilter::recursiveSmoothX(aImage2,0.8f);
		NFilter::recursiveSmoothY(aImage2,0.8f);

		// Mark areas sufficiently covered by tracks
		/*CMatrix<float> aCovered(mXSize,mYSize);
		aCovered = 1e20;

		CMatrix<float> aCorners;
		opflow::computeCorners(aImage1,aCorners,3.0f);
		float aCornerAvg = aCorners.avg();
		for (int ay = 4; ay < mYSize-4; ay+=mStep)
			for (int ax = 4; ax < mXSize-4; ax+=mStep) {
				if (aCovered(ax,ay) < mStep*mStep) continue;
				float distToImageBnd = exp(-0.1*NMath::min(NMath::min(NMath::min(ax,ay),mXSize-ax),mYSize-ay));
				if (aCorners(ax,ay) < 1.0* (aCornerAvg*(0.1+distToImageBnd))) continue;
				if (aCorners(ax,ay) < 1.0*(1.0f+distToImageBnd)) continue;
				mTracks.push_back(CTrack());
				CTrack& newTrack = mTracks.back();
				newTrack.mox = ax;
				newTrack.moy = ay;
				newTrack.mLabel = -1;
				newTrack.mSetupTime = tt;
			}*/

#ifdef BENCHMARK
t.start();
#endif
ldof(aImage1, aImage2, aForward, aBackward);
#ifdef BENCHMARK
t.stop();
std::cout << "(BENCHMARKING) Optical Flow: " << t.duration() << "ms" << std::endl;
#endif
// Check consistency of forward flow via backward flow
/*CMatrix<float> aUnreliable(mXSize,mYSize,0);
CTensor<float> dx(mXSize,mYSize,2);
CTensor<float> dy(mXSize,mYSize,2);
CDerivative<float> aDev(3);
NFilter::filter(aForward,dx,aDev,1,1);
NFilter::filter(aForward,dy,1,aDev,1);
CMatrix<float> aMotionEdge(mXSize,mYSize,0);
for (int i = 0; i < aSize; i++) {
	aMotionEdge.data()[i] += dx.data()[i]*dx.data()[i];
	aMotionEdge.data()[i] += dx.data()[aSize+i]*dx.data()[aSize+i];
	aMotionEdge.data()[i] += dy.data()[i]*dy.data()[i];
	aMotionEdge.data()[i] += dy.data()[aSize+i]*dy.data()[aSize+i];
}
for (int ay = 0; ay < aForward.ySize(); ay++)
	for (int ax = 0; ax < aForward.xSize(); ax++) {
		float bx = ax+aForward(ax,ay,0);
		float by = ay+aForward(ax,ay,1);
		int x1 = floor(bx);
		int y1 = floor(by);
		int x2 = x1+1;
		int y2 = y1+1;
		if (x1 < 0 || x2 >= mXSize || y1 < 0 || y2 >= mYSize) { aUnreliable(ax,ay) = 1.0f; continue;}
		float alphaX = bx-x1; float alphaY = by-y1;
		float a = (1.0-alphaX)*aBackward(x1,y1,0)+alphaX*aBackward(x2,y1,0);
		float b = (1.0-alphaX)*aBackward(x1,y2,0)+alphaX*aBackward(x2,y2,0);
		float u = (1.0-alphaY)*a+alphaY*b;
		a = (1.0-alphaX)*aBackward(x1,y1,1)+alphaX*aBackward(x2,y1,1);
		b = (1.0-alphaX)*aBackward(x1,y2,1)+alphaX*aBackward(x2,y2,1);
		float v = (1.0-alphaY)*a+alphaY*b;
		float cx = bx+u;
		float cy = by+v;
		float u2 = aForward(ax,ay,0);
		float v2 = aForward(ax,ay,1);
		if (((cx-ax)*(cx-ax)+(cy-ay)*(cy-ay)) >= 0.01*(u2*u2+v2*v2+u*u+v*v)+0.5f) { aUnreliable(ax,ay) = 1.0f; continue;}
		if (aMotionEdge(ax,ay) > 0.01*(u2*u2+v2*v2)+0.002f) { aUnreliable(ax,ay) = 1.0f; continue;}
	}
opflow::OFTensor<float> aShow(aImage2);
for (unsigned int i = 0; i < mTracks.size(); i++) {
	if (mTracks[i].mStopped) continue;
	float ax,ay,oldvar;
	if (mTracks[i].mSetupTime == tt) {
		ax = mTracks[i].mox; ay = mTracks[i].moy;
	}
	else {
		ax = mTracks[i].mx.back(); ay = mTracks[i].my.back();
	}
	int iax = lroundf(ax);
	int iay = lroundf(ay);
	if (aUnreliable(iax,iay) > 0) mTracks[i].mStopped = true;
	else {
		float bx = ax+aForward(iax,iay,0);
		float by = ay+aForward(iax,iay,1);
		int ibx = lroundf(bx);
		int iby = lroundf(by);
		if (ibx < 0 || iby < 0 || ibx >= mXSize || iby >= mYSize) mTracks[i].mStopped = true;
		else {
			mTracks[i].mx.push_back(bx);
			mTracks[i].my.push_back(by);
			mTracks[i].mLabel = 0;
			int t = mTracks[i].mx.size();
			int f = 1300/mSequenceLength;
			f=10;
			color.data()[0] = colour_code(0,f*t);
			color.data()[1] = colour_code(1,f*t);
			color.data()[2] = colour_code(2,f*t);

			//aShow.drawRect(ibx-1,iby-1,ibx+1,iby+1,colour_code(0,4*t),colour_code(1,4*t),colour_code(2,4*t));
			//aShow.drawRect(ibx-1,iby-1,ibx+1,iby+1,colour_code(0,f*t),colour_code(1,f*t),colour_code(2,f*t));
			// draw colored square
			{
				int x1 = std::max(0, ibx - 1);
				int y1 = std::max(0, iby - 1);
				int x2 = std::min(mXSize - 1, ibx + 1);
				int y2 = std::min(mYSize - 1, iby + 1);
				aShow.fillRect(color, x1, y1, x2, y2);
			}

		}
	}
}*/

		cv::Mat image_f(cv::Size(aForward.xSize(), aForward.ySize()), CV_32F);
		cv::Mat image_b(cv::Size(aBackward.xSize(), aBackward.ySize()), CV_32F);
		cv::Mat image_of(cv::Size(aBackward.xSize(), aBackward.ySize()), CV_32F);

		//aShow.writeToMat(image_of);
		aForward.writeToMat(image_f);
		aBackward.writeToMat(image_b);
		opflow::merge_flows(image_f, image_b, image_of);

		//cv::normalize(image_b, depth_image, 0.0, 255.0, cv::NORM_MINMAX);
		cv::normalize(image_f, depth_image, 0.0, 255.0, cv::NORM_MINMAX);
		cv::imshow("Image Forward", image_f);
		cv::imshow("Image Backward", image_b);
		cv::imshow("OF", image_of);
		cv::waitKey(0);

		aImage1 = aImage2;
	}

	//	aForward.writeToPGM("aForward.pgm");
	//	aBackward.writeToPGM("aBackward.pgm");

	return 0;
}