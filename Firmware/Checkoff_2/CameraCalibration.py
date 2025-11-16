import cv2
import numpy as np
import glob
import os

# ============================
# CHESSBOARD SETTINGS
# ============================
# For 320x240, use SMALLER chessboard patterns
PATTERNS_TO_TRY = [
    (7, 5),   # 8x6 squares - RECOMMENDED for low res
    (6, 4),   # 7x5 squares - even smaller
    (8, 6),   # 9x7 squares
    (5, 4),   # 6x5 squares - very small
    (6, 5),   # 7x6 squares
]

square_size = 0.025      # meters, change if different

# ADJUSTED criteria for low resolution
criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)

# ============================
# IMAGE FILES
# ============================
left_files = sorted(glob.glob('captured_images/camera1/*.jpg'))
right_files = sorted(glob.glob('captured_images/camera2/*.jpg'))

assert len(left_files) == len(right_files), "Unequal number of left/right images!"

print(f"Found {len(left_files)} image pairs (320x240 resolution)")
print("="*60)

# ============================
# AUTO-DETECT CORRECT PATTERN SIZE
# ============================
print("\n🔍 AUTO-DETECTING CHESSBOARD PATTERN SIZE (optimized for 320x240)...\n")

best_pattern = None
best_success_count = 0

for pattern_size in PATTERNS_TO_TRY:
    print(f"Trying pattern size: {pattern_size} ({pattern_size[0]+1}x{pattern_size[1]+1} squares)")
    success_count = 0
    
    # Test on first 5 pairs (or all if less than 5)
    test_pairs = min(5, len(left_files))
    
    for lf, rf in zip(left_files[:test_pairs], right_files[:test_pairs]):
        imgL = cv2.imread(lf, cv2.IMREAD_GRAYSCALE)
        imgR = cv2.imread(rf, cv2.IMREAD_GRAYSCALE)
        
        # MORE AGGRESSIVE FLAGS for low resolution
        flags = (cv2.CALIB_CB_ADAPTIVE_THRESH + 
                 cv2.CALIB_CB_NORMALIZE_IMAGE + 
                 cv2.CALIB_CB_FILTER_QUADS)
        
        retL, _ = cv2.findChessboardCorners(imgL, pattern_size, flags)
        retR, _ = cv2.findChessboardCorners(imgR, pattern_size, flags)
        
        if retL and retR:
            success_count += 1
    
    print(f"  ✓ Detected in {success_count}/{test_pairs} image pairs")
    
    if success_count > best_success_count:
        best_success_count = success_count
        best_pattern = pattern_size

if best_pattern is None:
    print("\n❌ ERROR: Could not detect chessboard in any images!")
    print("\n⚠️  CRITICAL ISSUE: 320x240 is very low resolution for chessboard detection!")
    print("\nRecommendations:")
    print("1. Use a SMALLER chessboard (6x5 or 7x5 squares)")
    print("2. Fill MORE of the frame with the chessboard")
    print("3. Ensure VERY HIGH CONTRAST (perfect lighting, no glare)")
    print("4. Use HIGHER resolution if possible (640x480 minimum recommended)")
    print("5. Make sure board is PERFECTLY FLAT and not tilted")
    print("\nShowing first image pair for inspection...")
    
    # Show first pair for manual inspection
    if len(left_files) > 0:
        imgL = cv2.imread(left_files[0], cv2.IMREAD_GRAYSCALE)
        imgR = cv2.imread(right_files[0], cv2.IMREAD_GRAYSCALE)
        
        # Upscale for viewing
        imgL_big = cv2.resize(imgL, (640, 480), interpolation=cv2.INTER_CUBIC)
        imgR_big = cv2.resize(imgR, (640, 480), interpolation=cv2.INTER_CUBIC)
        
        combined = np.hstack([imgL_big, imgR_big])
        cv2.imshow("First Image Pair (320x240 upscaled) - Press any key", combined)
        cv2.waitKey(0)
        cv2.destroyAllWindows()
    
    exit(1)

pattern_size = best_pattern
print(f"\n✅ BEST PATTERN: {pattern_size} ({pattern_size[0]+1}x{pattern_size[1]+1} squares)")
print("="*60)

# prepare object points
objp = np.zeros((pattern_size[0]*pattern_size[1], 3), np.float32)
objp[:, :2] = np.indices(pattern_size).T.reshape(-1, 2)
objp *= square_size

objpoints = []       # 3D points in real world space
imgpoints_left = []  # 2D points in left camera
imgpoints_right = [] # 2D points in right camera

# ============================
# DETECT CHESSBOARD CORNERS (OPTIMIZED FOR LOW RES)
# ============================
print("\n🔍 DETECTING CORNERS IN ALL IMAGES...\n")

first_success_img = None
success_count = 0
failure_count = 0

for idx, (lf, rf) in enumerate(zip(left_files, right_files), 1):
    imgL = cv2.imread(lf, cv2.IMREAD_GRAYSCALE)
    imgR = cv2.imread(rf, cv2.IMREAD_GRAYSCALE)
    
    if imgL is None or imgR is None:
        print(f"❌ [{idx}/{len(left_files)}] Could not read images!")
        continue

    # Verify image size
    if imgL.shape != (240, 320):
        print(f"⚠️  [{idx}/{len(left_files)}] Unexpected size: {imgL.shape}")

    # OPTIMIZED FLAGS for low resolution
    flags = (cv2.CALIB_CB_ADAPTIVE_THRESH + 
             cv2.CALIB_CB_NORMALIZE_IMAGE + 
             cv2.CALIB_CB_FILTER_QUADS)
    
    retL, cornersL = cv2.findChessboardCorners(imgL, pattern_size, flags)
    retR, cornersR = cv2.findChessboardCorners(imgR, pattern_size, flags)

    # Status for this pair
    status_left = "✓" if retL else "✗"
    status_right = "✓" if retR else "✗"
    
    if retL and retR:
        # Refine corners with SMALLER window for low res
        cornersL = cv2.cornerSubPix(imgL, cornersL, (5,5), (-1,-1), criteria)
        cornersR = cv2.cornerSubPix(imgR, cornersR, (5,5), (-1,-1), criteria)

        objpoints.append(objp)
        imgpoints_left.append(cornersL)
        imgpoints_right.append(cornersR)

        if first_success_img is None:
            first_success_img = imgL.copy()

        success_count += 1
        print(f"✅ [{idx}/{len(left_files)}] {os.path.basename(lf)}: L:{status_left} R:{status_right} - SUCCESS")

        # Visualize detected corners (upscale for better viewing)
        imgL_color = cv2.cvtColor(imgL, cv2.COLOR_GRAY2BGR)
        imgR_color = cv2.cvtColor(imgR, cv2.COLOR_GRAY2BGR)
        cv2.drawChessboardCorners(imgL_color, pattern_size, cornersL, retL)
        cv2.drawChessboardCorners(imgR_color, pattern_size, cornersR, retR)
        
        # Upscale 2x for display
        imgL_color = cv2.resize(imgL_color, (640, 480), interpolation=cv2.INTER_NEAREST)
        imgR_color = cv2.resize(imgR_color, (640, 480), interpolation=cv2.INTER_NEAREST)
        
        combined = np.hstack([imgL_color, imgR_color])
        cv2.imshow("Detected Corners (Left | Right) [320x240 upscaled]", combined)
        cv2.waitKey(100)

    else:
        failure_count += 1
        print(f"⚠️  [{idx}/{len(left_files)}] {os.path.basename(lf)}: L:{status_left} R:{status_right} - FAILED")
        
        # Show failed images (upscaled)
        imgL_color = cv2.cvtColor(imgL, cv2.COLOR_GRAY2BGR)
        imgR_color = cv2.cvtColor(imgR, cv2.COLOR_GRAY2BGR)
        
        # Try to show what detection sees
        imgL_big = cv2.resize(imgL_color, (640, 480), interpolation=cv2.INTER_CUBIC)
        imgR_big = cv2.resize(imgR_color, (640, 480), interpolation=cv2.INTER_CUBIC)
        
        cv2.putText(imgL_big, "DETECTION FAILED", (10, 30), 
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
        cv2.putText(imgR_big, "DETECTION FAILED", (10, 30), 
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
        
        combined = np.hstack([imgL_big, imgR_big])
        cv2.imshow("Failed Detection (Left | Right) - Press any key", combined)
        cv2.waitKey(500)

cv2.destroyAllWindows()

print("\n" + "="*60)
print(f"📊 DETECTION SUMMARY:")
print(f"  ✅ Successful pairs: {success_count}/{len(left_files)}")
print(f"  ❌ Failed pairs: {failure_count}/{len(left_files)}")
print(f"  Success rate: {success_count/len(left_files)*100:.1f}%")
print("="*60)

if len(objpoints) == 0:
    print("\n❌ ERROR: No chessboard corners detected in any image!")
    print("\n⚠️  320x240 RESOLUTION LIMITATIONS:")
    print("   • Very challenging for chessboard detection")
    print("   • Requires OPTIMAL conditions:")
    print("     - Smaller chessboard pattern (6x5 or 7x5 squares)")
    print("     - Chessboard fills 70-80% of frame")
    print("     - Perfect lighting with high contrast")
    print("     - No blur, glare, or shadows")
    print("     - Board perfectly flat and parallel to camera")
    exit(1)

if success_count < 8:
    print(f"\n⚠️  WARNING: Only {success_count} successful pairs.")
    print("   For 320x240, recommend at least 8-10 good pairs for calibration.")
    response = input("   Continue anyway? (y/n): ")
    if response.lower() != 'y':
        exit(0)

# ============================
# IMAGE SIZE
# ============================
h, w = first_success_img.shape[:2]
print(f"\nImage size: {w}x{h}")

# ============================
# CALIBRATE INDIVIDUAL CAMERAS
# ============================
print("\n📷 CALIBRATING INDIVIDUAL CAMERAS...")

retL, K1, D1, rvecs1, tvecs1 = cv2.calibrateCamera(
    objpoints, imgpoints_left, (w,h), None, None
)
retR, K2, D2, rvecs2, tvecs2 = cv2.calibrateCamera(
    objpoints, imgpoints_right, (w,h), None, None
)

print(f"  Left camera RMS error: {retL:.4f}")
print(f"  Right camera RMS error: {retR:.4f}")

if retL > 1.0 or retR > 1.0:
    print(f"\n⚠️  WARNING: High RMS error (>1.0) - calibration quality may be poor!")
    print("   This is common with 320x240 images. Consider higher resolution if possible.")

# ============================
# STEREO CALIBRATION
# ============================
print("\n🔗 PERFORMING STEREO CALIBRATION...")

flags = cv2.CALIB_FIX_INTRINSIC
criteria_stereo = (cv2.TERM_CRITERIA_MAX_ITER + cv2.TERM_CRITERIA_EPS, 100, 1e-5)

ret_stereo, K1, D1, K2, D2, R, T, E, F = cv2.stereoCalibrate(
    objpoints, imgpoints_left, imgpoints_right,
    K1, D1, K2, D2,
    (w, h),
    criteria=criteria_stereo,
    flags=flags
)

print(f"  Stereo RMS error: {ret_stereo:.4f}")

if ret_stereo > 1.0:
    print(f"  ⚠️  WARNING: High stereo error - may affect depth accuracy")

print(f"\n✅ CALIBRATION COMPLETE!")
print("="*60)

# ============================
# DISPLAY ALL CALIBRATION PARAMETERS
# ============================
print("\n" + "="*60)
print("📐 COMPLETE CALIBRATION PARAMETERS")
print("="*60)

print("\n🔹 LEFT CAMERA INTRINSIC MATRIX (K1):")
print(K1)

print("\n🔹 LEFT CAMERA DISTORTION COEFFICIENTS (D1):")
print("   [k1, k2, p1, p2, k3]")
print(D1.ravel())

print("\n🔹 RIGHT CAMERA INTRINSIC MATRIX (K2):")
print(K2)

print("\n🔹 RIGHT CAMERA DISTORTION COEFFICIENTS (D2):")
print("   [k1, k2, p1, p2, k3]")
print(D2.ravel())

print("\n🔹 ROTATION MATRIX (R) - Left to Right camera:")
print(R)

print("\n🔹 TRANSLATION VECTOR (T) - Left to Right camera [meters]:")
print(T.ravel())

# ============================
# RECTIFICATION + PROJECTION MATRICES
# ============================
R1, R2, P1, P2, Q, roi1, roi2 = cv2.stereoRectify(
    K1, D1, K2, D2, (w,h), R, T, alpha=0
)

print("\n🔹 LEFT RECTIFICATION MATRIX (R1):")
print(R1)

print("\n🔹 RIGHT RECTIFICATION MATRIX (R2):")
print(R2)

print("\n🔹 LEFT PROJECTION MATRIX (P1):")
print(P1)

print("\n🔹 RIGHT PROJECTION MATRIX (P2):")
print(P2)

print("\n🔹 DISPARITY-TO-DEPTH MATRIX (Q):")
print(Q)

# baseline in meters
baseline = abs(T[0])  # More robust for 320x240
print(f"\n📏 BASELINE (distance between cameras): {baseline:.4f} meters ({baseline*100:.2f} cm)")

# Extract focal length from intrinsic matrix
fx = K1[0, 0]
fy = K1[1, 1]
cx = K1[0, 2]
cy = K1[1, 2]

print(f"\n📷 CAMERA PARAMETERS SUMMARY:")
print(f"   Focal length (fx): {fx:.2f} pixels")
print(f"   Focal length (fy): {fy:.2f} pixels")
print(f"   Principal point (cx, cy): ({cx:.2f}, {cy:.2f})")
print(f"   Image center (expected): ({w/2:.1f}, {h/2:.1f})")
print(f"   Baseline: {baseline*100:.2f} cm")

print("\n" + "="*60)

# ============================
# RECTIFICATION MAPS
# ============================
map1x, map1y = cv2.initUndistortRectifyMap(K1, D1, R1, P1, (w,h), cv2.CV_32FC1)
map2x, map2y = cv2.initUndistortRectifyMap(K2, D2, R2, P2, (w,h), cv2.CV_32FC1)

print("\n✅ Rectification maps created successfully.")

# ============================
# SAVE CALIBRATION RESULTS
# ============================
output_dir = "calibration_output"
os.makedirs(output_dir, exist_ok=True)

np.savez(f"{output_dir}/stereo_calibration_320x240.npz",
         K1=K1, D1=D1, K2=K2, D2=D2,
         R=R, T=T, E=E, F=F,
         R1=R1, R2=R2, P1=P1, P2=P2, Q=Q,
         map1x=map1x, map1y=map1y,
         map2x=map2x, map2y=map2y,
         baseline=baseline,
         image_size=(w, h),
         pattern_size=pattern_size,
         square_size=square_size,
         rms_errors={'left': retL, 'right': retR, 'stereo': ret_stereo})

print(f"\n💾 Calibration data saved to: {output_dir}/stereo_calibration_320x240.npz")

# Save a detailed summary text file
with open(f"{output_dir}/calibration_summary.txt", 'w') as f:
    f.write(f"Stereo Camera Calibration Summary (320x240)\n")
    f.write(f"=" * 70 + "\n\n")
    f.write(f"Pattern size: {pattern_size} ({pattern_size[0]+1}x{pattern_size[1]+1} squares)\n")
    f.write(f"Square size: {square_size} meters\n")
    f.write(f"Successful image pairs: {success_count}\n\n")
    
    f.write(f"RMS ERRORS:\n")
    f.write(f"  Left camera: {retL:.4f}\n")
    f.write(f"  Right camera: {retR:.4f}\n")
    f.write(f"  Stereo: {ret_stereo:.4f}\n\n")
    
    f.write(f"BASELINE:\n")
    f.write(f"  {baseline:.4f} meters ({baseline*100:.2f} cm)\n\n")
    
    f.write(f"LEFT CAMERA INTRINSICS (K1):\n")
    f.write(f"{K1}\n\n")
    
    f.write(f"LEFT CAMERA DISTORTION (D1):\n")
    f.write(f"{D1.ravel()}\n\n")
    
    f.write(f"RIGHT CAMERA INTRINSICS (K2):\n")
    f.write(f"{K2}\n\n")
    
    f.write(f"RIGHT CAMERA DISTORTION (D2):\n")
    f.write(f"{D2.ravel()}\n\n")
    
    f.write(f"ROTATION MATRIX (R):\n")
    f.write(f"{R}\n\n")
    
    f.write(f"TRANSLATION VECTOR (T):\n")
    f.write(f"{T.ravel()}\n\n")
    
    f.write(f"LEFT RECTIFICATION MATRIX (R1):\n")
    f.write(f"{R1}\n\n")
    
    f.write(f"RIGHT RECTIFICATION MATRIX (R2):\n")
    f.write(f"{R2}\n\n")
    
    f.write(f"LEFT PROJECTION MATRIX (P1):\n")
    f.write(f"{P1}\n\n")
    
    f.write(f"RIGHT PROJECTION MATRIX (P2):\n")
    f.write(f"{P2}\n\n")
    
    f.write(f"DISPARITY-TO-DEPTH MATRIX (Q):\n")
    f.write(f"{Q}\n\n")

print(f"📄 Detailed summary saved to: {output_dir}/calibration_summary.txt")
print("\n🎉 ALL DONE!")