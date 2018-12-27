#include "ImageXyzcCuda.h"

#include "CudaUtilities.h"
#include "ImageXYZC.h"
#include "Logging.h"
#include "gl/Util.h"

#include "gl/Util.h"

#include <QElapsedTimer>
#include <QtDebug>

void
ChannelCuda::allocGpu(ImageXYZC* img, int channel, bool do_volume, bool do_gradient_volume)
{
  cudaExtent volumeSize;
  volumeSize.width = img->sizeX();
  volumeSize.height = img->sizeY();
  volumeSize.depth = img->sizeZ();

  Channelu16* ch = img->channel(channel);

  // intensity buffer
  if (do_volume) {
    // assuming 16-bit data!
    cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc(16, 0, 0, 0, cudaChannelFormatKindUnsigned);

    // create 3D array
    HandleCudaError(cudaMalloc3DArray(&m_volumeArray, &channelDesc, volumeSize));

    m_gpuBytes += (channelDesc.x + channelDesc.y + channelDesc.z + channelDesc.w) / 8 * volumeSize.width *
                  volumeSize.height * volumeSize.depth;

    // copy data to 3D array
    cudaMemcpy3DParms copyParams = { 0 };
    copyParams.srcPtr =
      make_cudaPitchedPtr(ch->m_ptr, volumeSize.width * img->sizeOfElement(), volumeSize.width, volumeSize.height);
    copyParams.dstArray = m_volumeArray;
    copyParams.extent = volumeSize;
    copyParams.kind = cudaMemcpyHostToDevice;
    HandleCudaError(cudaMemcpy3D(&copyParams));

    // create a texture object
    cudaResourceDesc texRes;
    memset(&texRes, 0, sizeof(cudaResourceDesc));
    texRes.resType = cudaResourceTypeArray;
    texRes.res.array.array = m_volumeArray;
    cudaTextureDesc texDescr;
    memset(&texDescr, 0, sizeof(cudaTextureDesc));
    texDescr.normalizedCoords = 1;
    texDescr.filterMode = cudaFilterModeLinear;
    texDescr.addressMode[0] = cudaAddressModeClamp; // clamp
    texDescr.addressMode[1] = cudaAddressModeClamp;
    texDescr.addressMode[2] = cudaAddressModeClamp;
    texDescr.readMode = cudaReadModeNormalizedFloat;
    HandleCudaError(cudaCreateTextureObject(&m_volumeTexture, &texRes, &texDescr, NULL));
  }

  // gradient buffer

  if (do_gradient_volume) {
    // assuming 16-bit data!
    cudaChannelFormatDesc gradientChannelDesc = cudaCreateChannelDesc(16, 0, 0, 0, cudaChannelFormatKindUnsigned);

    // create 3D array
    HandleCudaError(cudaMalloc3DArray(&m_volumeGradientArray, &gradientChannelDesc, volumeSize));
    m_gpuBytes += (gradientChannelDesc.x + gradientChannelDesc.y + gradientChannelDesc.z + gradientChannelDesc.w) / 8 *
                  volumeSize.width * volumeSize.height * volumeSize.depth;

    // copy data to 3D array
    cudaMemcpy3DParms gradientCopyParams = { 0 };
    gradientCopyParams.srcPtr = make_cudaPitchedPtr(
      ch->m_gradientMagnitudePtr, volumeSize.width * img->sizeOfElement(), volumeSize.width, volumeSize.height);
    gradientCopyParams.dstArray = m_volumeGradientArray;
    gradientCopyParams.extent = volumeSize;
    gradientCopyParams.kind = cudaMemcpyHostToDevice;
    HandleCudaError(cudaMemcpy3D(&gradientCopyParams));

    // create a texture object
    cudaResourceDesc gradientTexRes;
    memset(&gradientTexRes, 0, sizeof(cudaResourceDesc));
    gradientTexRes.resType = cudaResourceTypeArray;
    gradientTexRes.res.array.array = m_volumeGradientArray;
    cudaTextureDesc gradientTexDescr;
    memset(&gradientTexDescr, 0, sizeof(cudaTextureDesc));
    gradientTexDescr.normalizedCoords = 1;
    gradientTexDescr.filterMode = cudaFilterModeLinear;
    gradientTexDescr.addressMode[0] = cudaAddressModeClamp; // clamp
    gradientTexDescr.addressMode[1] = cudaAddressModeClamp;
    gradientTexDescr.addressMode[2] = cudaAddressModeClamp;
    gradientTexDescr.readMode = cudaReadModeNormalizedFloat;
    HandleCudaError(cudaCreateTextureObject(&m_volumeGradientTexture, &gradientTexRes, &gradientTexDescr, NULL));
  }

  // LUT buffer

  const int LUT_SIZE = 256;
  cudaChannelFormatDesc lutChannelDesc = cudaCreateChannelDesc(32, 0, 0, 0, cudaChannelFormatKindFloat);
  // create 1D array
  HandleCudaError(cudaMallocArray(&m_volumeLutArray, &lutChannelDesc, LUT_SIZE, 1));
  m_gpuBytes += (lutChannelDesc.x + lutChannelDesc.y + lutChannelDesc.z + lutChannelDesc.w) / 8 * LUT_SIZE;
  // copy data to 1D array
  HandleCudaError(cudaMemcpyToArray(m_volumeLutArray, 0, 0, ch->m_lut, LUT_SIZE * 4, cudaMemcpyHostToDevice));

  // create texture objects
  cudaResourceDesc lutTexRes;
  memset(&lutTexRes, 0, sizeof(cudaResourceDesc));
  lutTexRes.resType = cudaResourceTypeArray;
  lutTexRes.res.array.array = m_volumeLutArray;
  cudaTextureDesc lutTexDescr;
  memset(&lutTexDescr, 0, sizeof(cudaTextureDesc));
  lutTexDescr.normalizedCoords = 1;
  lutTexDescr.filterMode = cudaFilterModeLinear;
  lutTexDescr.addressMode[0] = cudaAddressModeClamp; // clamp
  lutTexDescr.addressMode[1] = cudaAddressModeClamp;
  lutTexDescr.addressMode[2] = cudaAddressModeClamp;
  lutTexDescr.readMode = cudaReadModeElementType; // direct read the (filtered) value
  HandleCudaError(cudaCreateTextureObject(&m_volumeLutTexture, &lutTexRes, &lutTexDescr, NULL));
}

void
ChannelCuda::deallocGpu()
{
  HandleCudaError(cudaDestroyTextureObject(m_volumeLutTexture));
  m_volumeLutTexture = 0;
  HandleCudaError(cudaDestroyTextureObject(m_volumeGradientTexture));
  m_volumeGradientTexture = 0;
  HandleCudaError(cudaDestroyTextureObject(m_volumeTexture));
  m_volumeTexture = 0;

  HandleCudaError(cudaFreeArray(m_volumeLutArray));
  m_volumeLutArray = nullptr;
  HandleCudaError(cudaFreeArray(m_volumeGradientArray));
  m_volumeGradientArray = nullptr;
  HandleCudaError(cudaFreeArray(m_volumeArray));
  m_volumeArray = nullptr;

  m_gpuBytes = 0;
}

void
ChannelCuda::updateLutGpu(int channel, ImageXYZC* img)
{
  static const int LUT_SIZE = 256;
  HandleCudaError(
    cudaMemcpyToArray(m_volumeLutArray, 0, 0, img->channel(channel)->m_lut, LUT_SIZE * 4, cudaMemcpyHostToDevice));
}

void
ImageCuda::allocGpu(ImageXYZC* img)
{
  deallocGpu();
  m_channels.clear();

  QElapsedTimer timer;
  timer.start();

  for (uint32_t i = 0; i < img->sizeC(); ++i) {
    ChannelCuda c;
    c.m_index = i;
    c.allocGpu(img, i);
    m_channels.push_back(c);
  }

  LOG_DEBUG << "allocGPU: Image to GPU in " << timer.elapsed() << "ms";
}

void
ImageCuda::createVolumeTexture4x16(ImageXYZC* img)
{
  // assuming 16-bit data!
  cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc(16, 16, 16, 16, cudaChannelFormatKindUnsigned);

  // create 3D array
  cudaExtent volumeSize;
  volumeSize.width = img->sizeX();
  volumeSize.height = img->sizeY();
  volumeSize.depth = img->sizeZ();

  m_gpuBytes += (channelDesc.x + channelDesc.y + channelDesc.z + channelDesc.w) / 8 * volumeSize.width *
                volumeSize.height * volumeSize.depth;

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glGenTextures(1, &m_VolumeGLTexture);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_3D, m_VolumeGLTexture);
  glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA16, img->sizeX(), img->sizeY(), img->sizeZ());
  glBindTexture(GL_TEXTURE_3D, 0);
  check_gl("volume texture creation");

  /////////////////////
  // use gl interop to let cuda read this tex.
  cudaGraphicsResource* cudaGLtexture = nullptr;
  HandleCudaError(
    cudaGraphicsGLRegisterImage(&cudaGLtexture, m_VolumeGLTexture, GL_TEXTURE_3D, cudaGraphicsRegisterFlagsReadOnly));

  HandleCudaError(cudaGraphicsMapResources(1, &cudaGLtexture));

  HandleCudaError(cudaGraphicsSubResourceGetMappedArray(&m_volumeArrayInterleaved, cudaGLtexture, 0, 0));

  cudaResourceDesc texRes;
  memset(&texRes, 0, sizeof(cudaResourceDesc));
  texRes.resType = cudaResourceTypeArray;
  texRes.res.array.array = m_volumeArrayInterleaved;
  cudaTextureDesc texDescr;
  memset(&texDescr, 0, sizeof(cudaTextureDesc));
  texDescr.normalizedCoords = 1;
  texDescr.filterMode = cudaFilterModeLinear;
  texDescr.addressMode[0] = cudaAddressModeClamp; // clamp
  texDescr.addressMode[1] = cudaAddressModeClamp;
  texDescr.addressMode[2] = cudaAddressModeClamp;
  texDescr.readMode = cudaReadModeNormalizedFloat;

  HandleCudaError(cudaCreateTextureObject(&m_volumeTextureInterleaved, &texRes, &texDescr, NULL));
  HandleCudaError(cudaGraphicsUnmapResources(1, &cudaGLtexture));
}

void
ImageCuda::updateVolumeData4x16(ImageXYZC* img, int c0, int c1, int c2, int c3)
{
  QElapsedTimer timer;
  timer.start();

  const int N = 4;
  int ch[4] = { c0, c1, c2, c3 };
  // interleaved all channels.
  // first 4.
  size_t xyz = img->sizeX() * img->sizeY() * img->sizeZ();
  uint16_t* v = new uint16_t[xyz * N];

  for (uint32_t i = 0; i < xyz; ++i) {
    for (int j = 0; j < N; ++j) {
      v[N * (i) + j] = img->channel(ch[j])->m_ptr[(i)];
    }
  }

  LOG_DEBUG << "Prepared interleaved hostmem buffer: " << timer.elapsed() << "ms";
  timer.start();

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_3D, m_VolumeGLTexture);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0,0,0, img->sizeX(), img->sizeY(), img->sizeZ(), GL_RGBA, GL_UNSIGNED_SHORT, v);
  glBindTexture(GL_TEXTURE_3D, 0);
  check_gl("update volume texture");

  LOG_DEBUG << "Copy volume to gpu: " << timer.elapsed() << "ms";

  delete[] v;
}

void
ImageCuda::allocGpuInterleaved(ImageXYZC* img)
{
  deallocGpu();
  m_channels.clear();

  QElapsedTimer timer;
  timer.start();

  createVolumeTexture4x16(img);
  uint32_t numChannels = img->sizeC();
  updateVolumeData4x16(
    img, 0, std::min(1u, numChannels - 1), std::min(2u, numChannels - 1), std::min(3u, numChannels - 1));

  for (uint32_t i = 0; i < numChannels; ++i) {
    ChannelCuda c;
    c.m_index = i;
    c.allocGpu(img, i, false, false);
    m_channels.push_back(c);

    m_gpuBytes += c.m_gpuBytes;
  }

  LOG_DEBUG << "allocGPUinterleaved: Image to GPU in " << timer.elapsed() << "ms";
  LOG_DEBUG << "allocGPUinterleaved: GPU bytes: " << m_gpuBytes;
}

void
ImageCuda::deallocGpu()
{
  for (size_t i = 0; i < m_channels.size(); ++i) {
    m_channels[i].deallocGpu();
  }
  HandleCudaError(cudaDestroyTextureObject(m_volumeTextureInterleaved));
  m_volumeTextureInterleaved = 0;
  HandleCudaError(cudaFreeArray(m_volumeArrayInterleaved));
  m_volumeArrayInterleaved = nullptr;
  
//  check_gl("delete gl volume tex");
//  glBindTexture(GL_TEXTURE_3D, 0);
//  check_gl("delete gl volume tex");
//  glDeleteTextures(1, &m_VolumeGLTexture);
//  check_gl("delete gl volume tex");
//  m_VolumeGLTexture = 0;

  m_gpuBytes = 0;
}

void
ImageCuda::updateLutGpu(int channel, ImageXYZC* img)
{
  m_channels[channel].updateLutGpu(channel, img);
}


void ChannelGL::allocGpu(ImageXYZC* img, int channel) {
    cudaExtent volumeSize;
    volumeSize.width = img->sizeX();
    volumeSize.height = img->sizeY();
    volumeSize.depth = img->sizeZ();


    Channelu16* ch = img->channel(channel);

    // LUT buffer

    glGenTextures(1, &_volumeLutTexture);
    updateLutGpu(channel, img);
}

void ChannelGL::deallocGpu() {
    glDeleteTextures(1, &_volumeLutTexture);
    _volumeLutTexture = 0;
    glDeleteTextures(1, &_volumeGradientTexture);
    _volumeGradientTexture = 0;
    glDeleteTextures(1, &_volumeTexture);
    _volumeTexture = 0;

    _gpuBytes = 0;
}

void ChannelGL::updateLutGpu(int channel, ImageXYZC* img) {
    static const int LUT_SIZE = 256;
    glBindTexture(GL_TEXTURE_2D, _volumeLutTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, LUT_SIZE, 1, 0, GL_RED, GL_FLOAT, img->channel(channel)->_lut);
    //HandleCudaError(cudaMemcpyToArray(_volumeLutArray, 0, 0, img->channel(channel)->_lut, LUT_SIZE * 4, cudaMemcpyHostToDevice));
}

void ImageGL::createVolumeTexture4x16(ImageXYZC* img, GLuint* volumeTexture) {
    // assuming 16-bit data!
    glGenTextures(1, volumeTexture);

    cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc(16, 16, 16, 16, cudaChannelFormatKindUnsigned);

    // create 3D array
    cudaExtent volumeSize;
    volumeSize.width = img->sizeX();
    volumeSize.height = img->sizeY();
    volumeSize.depth = img->sizeZ();
    _gpuBytes += (channelDesc.x + channelDesc.y + channelDesc.z + channelDesc.w) / 8 * volumeSize.width * volumeSize.height * volumeSize.depth;

}

void ImageGL::updateVolumeData4x16(ImageXYZC* img, int c0, int c1, int c2, int c3)
{
    QElapsedTimer timer;
    timer.start();

    // create gl 3d texture from imagexyzc and active channels
    uint32_t numChannels = img->sizeC();

    glGenTextures(1, &_volumeTextureInterleaved);
    check_gl("Gen volume texture id");
    glBindTexture(GL_TEXTURE_3D, _volumeTextureInterleaved);
    check_gl("Bind volume texture");

    const int N = 4;
    int ch[4] = { 0, std::min(1u, numChannels - 1), std::min(2u, numChannels - 1), std::min(3u, numChannels - 1) };
    // interleaved all channels.
    // first 4.
    size_t xyz = img->sizeX()*img->sizeY()*img->sizeZ();
    uint16_t* v = new uint16_t[xyz * N];

    for (uint32_t i = 0; i < xyz; ++i) {
        for (int j = 0; j < N; ++j) {
            v[N * (i)+j] = img->channel(ch[j])->_ptr[(i)];
        }
    }

    LOG_DEBUG << "Prepared interleaved hostmem buffer: " << timer.elapsed() << "ms";
    timer.start();

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16UI, img->sizeX(), img->sizeY(), img->sizeZ(), 0, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, v);
    check_gl("glTexImage3D");

    LOG_DEBUG << "Copy volume to gpu: " << timer.elapsed() << "ms";

    delete[] v;
}

void ImageGL::allocGpuInterleaved(ImageXYZC* img) {
    deallocGpu();
    _channels.clear();

    QElapsedTimer timer;
    timer.start();

    createVolumeTexture4x16(img, &_volumeTextureInterleaved);
    uint32_t numChannels = img->sizeC();
    updateVolumeData4x16(img, 0, std::min(1u, numChannels - 1), std::min(2u, numChannels - 1), std::min(3u, numChannels - 1));


    for (uint32_t i = 0; i < numChannels; ++i) {
        ChannelGL c;
        c._index = i;
        c.allocGpu(img, i);
        _channels.push_back(c);

        _gpuBytes += c._gpuBytes;
    }

    LOG_DEBUG << "allocGPUinterleaved: Image to GPU in " << timer.elapsed() << "ms";
    LOG_DEBUG << "allocGPUinterleaved: GPU bytes: " << _gpuBytes;

}

void ImageGL::deallocGpu() {
    for (size_t i = 0; i < _channels.size(); ++i) {
        _channels[i].deallocGpu();
    }
    glDeleteTextures(1, &_volumeTextureInterleaved);
    _volumeTextureInterleaved = 0;
    _gpuBytes = 0;
}

void ImageGL::updateLutGpu(int channel, ImageXYZC* img) {
    _channels[channel].updateLutGpu(channel, img);
}
