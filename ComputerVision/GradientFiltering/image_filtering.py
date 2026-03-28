"""
CSE 4310 Homework 1 Programming
"""

import cv2
import numpy as np
import matplotlib.pyplot as plt

# main function
if __name__ == '__main__':
    # Step 1: read the crack box image with cv2.imread
    im = cv2.imread('cracker_box.jpg')


    # Step 2: use cv2.cvtColor to convert RGB image to gray scale image 
    #(replace with your code)
    gray_image = cv2.cvtColor(im, cv2.COLOR_BGR2GRAY).astype(np.float32)


    # Step 3: define the filter kernel as described in the homework description as a numpy array
    #(replace with your code)
    kernel = np.array([[-1, 0, 1],[-2, 0, 2],[-1, 0, 1]], dtype=np.float32)


    # Step 4: filter the image with the kernel
    #(replace with your code)
    output = np.zeros_like(gray_image, dtype=np.float32)
    padding = np.pad(gray_image, 1, mode='constant', constant_values=0)

    h, w = gray_image.shape
    for i in range(1, h+1):
        for j in range(1, w+1):
            region = padding[i-1:i+2, j-1:j+2]
            output[i-1, j-1] = np.sum(region * kernel)

    # show result with matplotlib (no need to change code below)
    fig = plt.figure()
    ax = fig.add_subplot(1, 2, 1)
    plt.imshow(gray_image, cmap = 'gray')
    ax.set_title('Original image')

    ax = fig.add_subplot(1, 2, 2)
    plt.imshow(output, cmap = 'gray')
    ax.set_title('Filtered image')

    plt.show()
