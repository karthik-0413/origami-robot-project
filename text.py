import os
import cv2
import matplotlib.pyplot as plt

# Paths (edit if needed)
cam1_dir = "captured_images/camera1"
cam2_dir = "captured_images/camera2"

# Get sorted lists of images
cam1_images = sorted([os.path.join(cam1_dir, f) for f in os.listdir(cam1_dir) if f.lower().endswith(".jpg")])
cam2_images = sorted([os.path.join(cam2_dir, f) for f in os.listdir(cam2_dir) if f.lower().endswith(".jpg")])

# Make sure counts match
num_images = min(len(cam1_images), len(cam2_images))
print(f"Displaying {num_images} stereo pairs...")

plt.ion()  # interactive mode for live updating
fig, axes = plt.subplots(1, 2, figsize=(10, 5))

for i in range(num_images):
    img1 = cv2.imread(cam1_images[i])
    img2 = cv2.imread(cam2_images[i])

    # Convert BGR -> RGB for matplotlib
    img1 = cv2.cvtColor(img1, cv2.COLOR_BGR2RGB)
    img2 = cv2.cvtColor(img2, cv2.COLOR_BGR2RGB)

    axes[0].imshow(img1)
    axes[0].set_title(f"Camera 1")
    axes[0].axis("off")

    axes[1].imshow(img2)
    axes[1].set_title(f"Camera 2")
    axes[1].axis("off")

    plt.tight_layout()
    plt.pause(0.3)   # speed of slideshow (0.1s = 10 FPS)

plt.ioff()
plt.show()
