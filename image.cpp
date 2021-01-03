#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <tiff.h>
#include <tiffio.h>
#include <omp.h>

#define CLAMP(x) ((x)>0?(((x)<255)?(x):255):0)
#define TIME std::chrono::system_clock::time_point

static void filter(const unsigned char* in, unsigned char* out, const int width, const int height, const float* kernel, const int kernel_size)
{
  const int kernel_half_size = kernel_size / 2;

  #pragma omp parallel for if(height<100)
  for (int y = kernel_half_size; y < height - kernel_half_size; y++)
  {
    for (int x = kernel_half_size; x < width - kernel_half_size; x++)
    {
      for (int k = 0; k < 3; k++)
      {
        float data = 0.0;
        for (int fy = 0; fy < kernel_size; fy++)
        {
          int iy = y - kernel_half_size + fy;
          for (int fx = 0; fx < kernel_size; fx++)
          {
            int ix = x - kernel_half_size + fx;
            data += kernel[fy * kernel_size + fx] * in[(iy * width + ix) * 3 + k];
          }
        }
        out[(y * width + x) * 3 + k] = CLAMP(data);
      }
    }
  }
}

int main(int argc, char *argv[])
{
  TIFF *tiff;
  unsigned int length = 0;
  unsigned int width = 0;
  unsigned int bitpersample = 0;
  unsigned int ch = 0;
  unsigned int *rgbaData = NULL;
  unsigned char *rgbData = NULL;
  int i, j;

  tiff = TIFFOpen("test.tif", "r");
  if (tiff == NULL)
  {
    printf("TIFFOpen error\n");
    return -1;
  }

  if (!TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &length))
  {
    printf("TIFFGetField length error\n");
    TIFFClose(tiff);
    return -1;
  }

  if (!TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width))
  {
    printf("TIFFGetField width error\n");
    TIFFClose(tiff);
    return -1;
  }

  if (!TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bitpersample))
  {
    printf("TIFFGetField bitpersample error\n");
    TIFFClose(tiff);
    return -1;
  }

  if (!TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &ch))
  {
    printf("TIFFGetField samplesperpixel error\n");
    TIFFClose(tiff);
    return -1;
  }

  rgbaData = (unsigned int*) malloc(sizeof(unsigned int) * width * length);

  if (!TIFFReadRGBAImage(tiff, width, length, rgbaData, 0))
  {
    printf("TIFFReadRGBAImage error\n");
    free(rgbData);
    TIFFClose(tiff);
    return -1;
  }
  TIFFClose(tiff);

  int size = width * length * 3;
  rgbData = (unsigned char*) malloc(sizeof(unsigned char) * size);
  if (rgbData == NULL)
  {
    printf("malloc rgbData error\n");
    free(rgbaData);
    return -1;
  }

  for (int y = 0; y < length; y++)
  {
    for (int x = 0; x < width; x++)
    {
      rgbData[3 * (x + y * width) + 0] = TIFFGetR(rgbaData[x + y * width]);
      rgbData[3 * (x + y * width) + 1] = TIFFGetG(rgbaData[x + y * width]);
      rgbData[3 * (x + y * width) + 2] = TIFFGetB(rgbaData[x + y * width]);
    }
  }

  const float kernel[9] = {-1,-1,-1,-1,8,-1,-1,-1,-1};
  unsigned char* out = (unsigned char *) malloc(sizeof(unsigned char) * size);

  TIME start = std::chrono::system_clock::now();

  filter(rgbData, out, width, length, kernel, 3);

  TIME end = std::chrono::system_clock::now();
  long long msec = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  printf("elapsed time = %lld [msec]\n", msec);

  tiff = TIFFOpen("out.tif", "w");
  if (tiff == NULL)
  {
    printf("TIFFOpen error\n");
  }

  if (!TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE))
  {
    printf("TIFFSetField compression error\n");
  }

  if (!TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, width))
  {
    printf("TIFFSetField width error\n");
  }

  if (!TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, length))
  {
    printf("TIFFSetField length error\n");
  }

  if (!TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 8))
  {
    printf("TIFFSetField bitspersample error\n");
  }

  if (!TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 3))
  {
    printf("TIFFSetField samplesperpixel error\n");
  }

  if (!TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB))
  {
    printf("TIFFSetField photometric error\n");
  }

  if (!TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG))
  {
    printf("TIFFSetField planarconfig error\n");
  }

  if (!TIFFSetField(tiff, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB))
  {
    printf("TIFFSetField fillorder error\n");
  }

  if (!TIFFSetField(tiff, TIFFTAG_XRESOLUTION, 72.0))
  {
    printf("TIFFSetField xresolution error\n");
  }

  if (!TIFFSetField(tiff, TIFFTAG_YRESOLUTION, 72.0))
  {
    printf("TIFFSetField yresolution error\n");
  }

  if (!TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, length))
  {
    printf("TIFFSetField rowsperstrip error\n");
  }

  size = TIFFWriteEncodedStrip(tiff, 0, out, size);
  if (size == -1)
  {
    printf("TIFFWriteEncodedStrip error\n");
  }

  TIFFClose(tiff);

  if (rgbaData)
  {
    free(rgbaData);
    rgbaData = NULL;
  }

  if (rgbData)
  {
    free(rgbData);
    rgbData = NULL;
  }

  if (out)
  {
    free(out);
    out = NULL;
  }

  return 0;
}