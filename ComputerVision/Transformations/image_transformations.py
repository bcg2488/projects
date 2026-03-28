"""
CS 4310 Homework 3 Programming
Transform images
"""

import cv2
import numpy as np
import matplotlib.pyplot as plt


def _bilinear_interpolation(img, x, y):
    """
    Manually performs bilinear interpolation for a single pixel.

    Args:
        img (np.ndarray): The source image.
        x (np.ndarray): The x-coordinates for resampling.
        y (np.ndarray): The y-coordinates for resampling.

    Returns:
        np.ndarray: The interpolated pixel values.
    """
    height, width = img.shape[:2]

    # Find the four surrounding pixel coordinates (clamped to image bounds)
    x0 = np.floor(x).astype(int)
    x1 = x0 + 1
    y0 = np.floor(y).astype(int)
    y1 = y0 + 1

    # Clamp coordinates to prevent reading outside the image bounds
    x0 = np.clip(x0, 0, width - 1)
    x1 = np.clip(x1, 0, width - 1)
    y0 = np.clip(y0, 0, height - 1)
    y1 = np.clip(y1, 0, height - 1)

    # Get the four surrounding pixel values
    q11 = img[y0, x0]
    q12 = img[y1, x0]
    q21 = img[y0, x1]
    q22 = img[y1, x1]

    # Calculate interpolation weights
    dx = x - x0
    dy = y - y0
    dx = dx[:, np.newaxis]
    dy = dy[:, np.newaxis]

    # Calculate the weighted sum for bilinear interpolation
    val = q11 * (1 - dx) * (1 - dy) + \
          q21 * dx * (1 - dy) + \
          q12 * (1 - dx) * dy + \
          q22 * dx * dy

    return val


#TODO: implementation this function
# transform the input image im (H, W, 3) according to the 2D transformation T (3x3 matrix)
# the output is the transformed image with the same shape (H, W, 3)
# idea: compute the inverse transformation first and then use backward image warping to get the transformed image. The
# warping process should use bilinear interpolation. Out of boundary pixels after transformation should be removed from
# the image region. Absent pixels after transformation should be filled with zeros.

def transform(im, T):
    # compute the inverse transformation
    h, w = im.shape[:2]
    im_new = np.zeros_like(im)
    
    T_inv = np.linalg.inv(T)
    
    # bilinear interpolation
    for i in range(h):
        for j in range(w):
            src = T_inv @ np.array([j, i, 1])
            x, y= src[0]/src[2], src[1]/src[2]
            if 0 <= x < w-1 and 0 <= y < h-1:
                im_new[i, j] = _bilinear_interpolation(im, np.array([x]), np.array([y]))

    return im_new


# main function
# notice you cannot run this main function until you implement the above transform() function 
if __name__ == '__main__':

    # load the image in data
    filename = '000006-color.jpg'
    im = cv2.imread(filename)
    
    # image height and width
    height = im.shape[0]
    width = im.shape[1]
    
    # 2D translation
    T1 = np.eye(3, dtype=np.float32)
    T1[0, 2] = 50
    T1[1, 2] = 100
    im_1 = transform(im, T1)
    print('2D translation')
    print(T1)
    
    # 2D rotation
    R = cv2.getRotationMatrix2D((width/2, height/2), 45, 1)
    T2 = np.eye(3, dtype=np.float32)
    T2[:2, :] = R
    im_2 = transform(im, T2)
    print('2D rotation')
    print(T2)
    
    # 2D rigid transformation: 2D rotation + 2D transformation
    T3 = np.matmul(T1, T2)
    im_3 = transform(im, T3)
    print('2D rigid transform')
    print(T3)
    
    # 2D affine transformation
    pts1 = np.float32([[50,50], [200,50], [50,200]])
    pts2 = np.float32([[10,100], [200,50], [100,250]])
    M = cv2.getAffineTransform(pts1, pts2)
    T4 = np.eye(3, dtype=np.float32)
    T4[:2, :] = M
    print('Affine transform')
    print(T4)
    im_4 = transform(im, T4)
    
    # 2D perspective transformation
    pts1 = np.float32([[56,65], [368,52], [28,387], [389,390]])
    pts2 = np.float32([[0,0], [300,0], [0,300], [300,300]])
    T5 = cv2.getPerspectiveTransform(pts1, pts2)
    print('Perspective transform')
    print(T5)
    im_5 = transform(im, T5)
    
    # show the images
    fig = plt.figure(figsize=(27, 18))
    ax = fig.add_subplot(2, 3, 1)
    plt.imshow(im[:, :, (2, 1, 0)])        
    ax.set_title('original image')
    
    ax = fig.add_subplot(2, 3, 2)    
    plt.imshow(im_1[:, :, (2, 1, 0)])        
    ax.set_title('translated image')
    
    ax = fig.add_subplot(2, 3, 3)    
    plt.imshow(im_2[:, :, (2, 1, 0)])        
    ax.set_title('rotated image')
    
    ax = fig.add_subplot(2, 3, 4)    
    plt.imshow(im_3[:, :, (2, 1, 0)])
    ax.set_title('rigid transformed image')   
    
    ax = fig.add_subplot(2, 3, 5)
    plt.imshow(im_4[:, :, (2, 1, 0)])        
    ax.set_title('affine transformed image')
    
    ax = fig.add_subplot(2, 3, 6)    
    plt.imshow(im_5[:, :, (2, 1, 0)])        
    ax.set_title('perspective transformed image')

    plt.savefig('2D_image_transformation.pdf')
    plt.show()