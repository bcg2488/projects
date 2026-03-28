"""
CS 4310 Homework 2 Programming
Implement the edge_detection() function in this python script
Edge Detection
"""

import cv2
import numpy as np
import matplotlib.pyplot as plt


#TODO: implement this function
# input: im is an RGB image with shape [height, width, 3]
# output: edge_mask with shape [height, width] with valuse 0 and 1, where 1s indicate edge pixels of the input image
# You can use opencv functions and numpy functions
def edge_detection(im):

    # step 0: convert RGB to gray-scale image and normalize to [0.0, 1.0]
    gray_image = cv2.cvtColor(im, cv2.COLOR_BGR2GRAY).astype(np.float32) / 255.0
    
    # step 1: compute image gradient using Sobel filters with kernel size 5
    # https://opencv24-python-tutorials.readthedocs.io/en/latest/py_tutorials/py_imgproc/py_gradients/py_gradients.html
    sobel_x = cv2.Sobel(gray_image, cv2.CV_64F, 1, 0, ksize=5)
    sobel_y = cv2.Sobel(gray_image, cv2.CV_64F, 0, 1, ksize=5)

    # step 2: compute gradient magnitude at every pixels
    mag = np.sqrt(sobel_x**2 + sobel_y**2)
    
    # step 3: threshold the gradient magnitude to obtain edge mask
    # use threshold with value 5
    threshold = 5
    edge_mask = (mag > threshold).astype(np.uint8)

    return edge_mask


# main function
if __name__ == '__main__':

    # read the image in data
    # rgb image
    rgb_filename = 'cracker_box.jpg'
    im = cv2.imread(rgb_filename)
    
    # your implementation of the edge detector
    edge_mask = edge_detection(im)
        
    # visualization for your debugging
    fig = plt.figure(figsize=(18, 9))
        
    # show RGB image
    ax = fig.add_subplot(1, 2, 1)
    plt.imshow(im[:, :, (2, 1, 0)])
    ax.set_title('RGB image')
        
    # show your edge image
    ax = fig.add_subplot(1, 2, 2)
    plt.imshow(im[:, :, (2, 1, 0)])
    index = np.where(edge_mask > 0)
    plt.scatter(x=index[1], y=index[0], c='y',  s=0.1)
    ax.set_title('edge image')

    plt.show()