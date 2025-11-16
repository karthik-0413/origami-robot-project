import cv2
import numpy as np
import glob
import os

# chessboard settings (inner corners)
pattern_size = (9, 6)         # change to your pattern inner corners
square_size = 0.025           # meters (change to your square size)

# termination criteria for cornerSubPix & stereoCalibrate
criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 1e-6)

# prepare object points for one view: (0,0,0), (1,0,0), ... scaled by square_size
objp = np.zeros((pattern_size[0]*pattern_size[1], 3), np.float32)
objp[:, :2] = np.indices(pattern_size).T.reshape(-1, 2)
objp *= square_size

objpoints = []   # 3d points in real world space
imgpoints_left = []
imgpoints_right = []

left_files = sorted(glob.glob('left/*.png'))
right_files = sorted(glob.glob('right/*.png'))

assert len(left_files) == len(right_files), "Unequal number of left/right images"

for lf, rf in zip(left_files, right_files):
    imgL = cv2.imread(lf, cv2.IMREAD_GRAYSCALE)
    imgR = cv2.imread(rf, cv2.IMREAD_GRAYSCALE)

    retL, cornersL = cv2.findChessboardCorners(imgL, pattern_size, None)
    retR, cornersR = cv2.findChessboardCorners(imgR, pattern_size, None)

    if retL and retR:
        # refine corners to subpixel accuracy
        cornersL = cv2.cornerSubPix(imgL, cornersL, (11,11), (-1,-1), criteria)
        cornersR = cv2.cornerSubPix(imgR, cornersR, (11,11), (-1,-1), criteria)

        objpoints.append(objp)
        imgpoints_left.append(cornersL)
        imgpoints_right.append(cornersR)
    else:
        print("Skipping pair: ", os.path.basename(lf))

# image size
h, w = imgL.shape[:2]

# Option: calibrate each camera separately first (recommended)
retL, K1, D1, rvecs1, tvecs1 = cv2.calibrateCamera(objpoints, imgpoints_left, (w,h), None, None)
retR, K2, D2, rvecs2, tvecs2 = cv2.calibrateCamera(objpoints, imgpoints_right, (w,h), None, None)

print("Single camera reprojection errors:", retL, retR)

# stereo calibrate (fix intrinsics so we don't re-optimize them)
flags = cv2.CALIB_FIX_INTRINSIC
criteria_stereo = (cv2.TERM_CRITERIA_MAX_ITER + cv2.TERM_CRITERIA_EPS, 100, 1e-5)

ret_stereo, K1, D1, K2, D2, R, T, E, F = cv2.stereoCalibrate(
    objpoints, imgpoints_left, imgpoints_right,
    K1, D1, K2, D2,
    (w, h),
    criteria=criteria_stereo,
    flags=flags
)

print("stereoCalibrate RMS error:", ret_stereo)
print("R (rotation from left to right):\n", R)
print("T (translation from left to right):\n", T.ravel())

# compute rectification transforms and projection matrices
R1, R2, P1, P2, Q, roi1, roi2 = cv2.stereoRectify(
    K1, D1, K2, D2, (w,h), R, T, alpha=0  # alpha=0 -> crop valid pixels, alpha= -1..1 can be tuned
)

print("P1:\n", P1)
print("P2:\n", P2)
print("Q:\n", Q)

# baseline (meters)
# P2[0,3] usually equals -fx * Tx, so Tx = -P2[0,3] / P2[0,0]
fx = P2[0,0]
Tx = -P2[0,3] / fx
print("baseline (Tx) [units of object points, e.g. meters]:", Tx)

# build undistort/rectify maps for remapping images before stereo matching
map1x, map1y = cv2.initUndistortRectifyMap(K1, D1, R1, P1, (w,h), cv2.CV_32FC1)
map2x, map2y = cv2.initUndistortRectifyMap(K2, D2, R2, P2, (w,h), cv2.CV_32FC1)

# Example: rectify a pair and compute disparity (optional)
imgL = cv2.imread(left_files[0], cv2.IMREAD_GRAYSCALE)
imgR = cv2.imread(right_files[0], cv2.IMREAD_GRAYSCALE)
rectL = cv2.remap(imgL, map1x, map1y, cv2.INTER_LINEAR)
rectR = cv2.remap(imgR, map2x, map2y, cv2.INTER_LINEAR)

# compute disparity (simple example)
stereo = cv2.StereoSGBM_create(minDisparity=0, numDisparities=128, blockSize=7)
disp = stereo.compute(rectL, rectR).astype(np.float32) / 16.0

# reproject to 3D
points_3d = cv2.reprojectImageTo3D(disp, Q)
# mask for valid disparities
mask = (disp > disp.min()) & (disp < 10000)
# pick one valid point to inspect
ys, xs = np.where(mask)
if len(xs):
    x, y = xs[0], ys[0]
    X, Y, Z = points_3d[y, x]
    print("Example 3D point at pixel", (x,y), ":", X, Y, Z)
