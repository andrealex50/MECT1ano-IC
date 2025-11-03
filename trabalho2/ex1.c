#include <opencv2/core/core_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <input_image> <output_image> <channel_number>\n", argv[0]);
        printf("Channel numbers: 0=Blue, 1=Green, 2=Red (for BGR images)\n");
        return -1;
    }

    const char *input_filename = argv[1];
    const char *output_filename = argv[2];
    int channel = atoi(argv[3]);

    // validate channel number
    if (channel < 0 || channel > 2) {
        printf("Error: Channel number must be 0, 1, or 2\n");
        printf("0=Blue, 1=Green, 2=Red\n");
        return -1;
    }

    IplImage *src = cvLoadImage(input_filename, CV_LOAD_IMAGE_COLOR);
    if (!src) {
        printf("Error: Could not load image '%s'\n", input_filename);
        return -1;
    }

    printf("Image loaded successfully\n");
    printf("Width: %d, Height: %d, Channels: %d\n", 
           src->width, src->height, src->nChannels);

    // create output single-channel image
    IplImage *dst = cvCreateImage(cvGetSize(src), src->depth, 1);
    if (!dst) {
        printf("Error: Could not create output image\n");
        cvReleaseImage(&src);
        return -1;
    }

    printf("Extracting channel %d...\n", channel);
    
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            unsigned char *src_pixel = (unsigned char *)(src->imageData + y * src->widthStep + x * src->nChannels);
            
            unsigned char *dst_pixel = (unsigned char *)(dst->imageData + y * dst->widthStep + x);
            
            *dst_pixel = src_pixel[channel];
        }
    }

    if (cvSaveImage(output_filename, dst, 0)) {
        printf("Channel extracted successfully and saved to '%s'\n", output_filename);
    } else {
        printf("Error: Could not save image to '%s'\n", output_filename);
        cvReleaseImage(&src);
        cvReleaseImage(&dst);
        return -1;
    }

    cvReleaseImage(&src);
    cvReleaseImage(&dst);

    return 0;
}