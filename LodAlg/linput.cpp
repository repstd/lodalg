#include "stdafx.h"
#include "linput.h"
#include <iostream>
rawData::rawData() :dataImp()
{
}
void rawData::load(const char* filename, const char* driverName)
{
	FILE* fRaw = fopen(filename, "r");
	assert(fRaw != NULL);
	long sz;
	fseek(fRaw, 0, std::ios::end);
	sz = ftell(fRaw);
	fseek(fRaw, 0, std::ios::beg);
	fclose(fRaw);
	m_iSize = sz;
	m_iWidth = sqrt(double(sz));
	m_iHeight = m_iWidth;
}
void rawData::getExtent(extent& extent)
{
	extent._width = m_iWidth;
	extent._height = m_iHeight;
	extent._channel = 1;
}
void rawData::getTile(BYTE* src, BYTE* dst, int row, int col, int N)
{
	assert(row < N && row>0);
	assert(col< N && col>0);
	assert(src != NULL&& dst != NULL);
	extent ext = getExtent();
	int channel = ext._channel;
	int srcWidth = ext._width;
	int tileWidth = (srcWidth - 1) / N + 1;

	int tileWidthEven = srcWidth / N;

	int tileHeight = tileWidthEven;

	for (int r = row*tileHeight; r < row*tileHeight + tileHeight; r++)
	{
		memcpy(dst + (r - row*tileHeight)*tileWidthEven*channel, src + r*srcWidth*channel, tileWidth*channel*sizeof(BYTE));
	}
}
extent rawData::getExtent()
{
	extent ext;
	getExtent(ext);
	return ext;
}
void	rawDataProxy::load(const char* filename, const char* driverName)
{
	std::unique_ptr<rawData> temp(new rawData);
	m_rawData = std::move(temp);
	temp.release();
	getInputData()->load(filename);
	m_filename = filename;
}
void	rawDataProxy::getExtent(extent& extent)
{
	getInputData()->getExtent(extent);
}
void	rawDataProxy::getTile(BYTE* src, BYTE* dst, int row, int col, int N)
{
	getInputData()->getTile(src, dst, row, col, N);

}
void rawDataProxy::getTile(BYTE* dst, int row, int col, int N)
{
	FILE* fp = fopen(getFilename(), "r");
	extent ext = getExtent();
	BYTE* buf = new unsigned char[ext._width*ext._height*ext._channel];

	fread(buf, ext._width*ext._height*ext._channel, 1, fp);

	getTile(buf, dst, row, col, N);

	fclose(fp);
	delete[] buf;

}
extent  rawDataProxy::getExtent()
{
	return getInputData()->getExtent();
}
const char*   rawDataProxy::getFilename()
{
	return m_filename.c_str();
}
rawData* rawDataProxy::getInputData()
{
	return m_rawData.get();
}
rawDataProxy::rawDataProxy() :dataImp()
{

}


gdalData::gdalData() :dataImp()
{
}
void gdalData::load(const char* filename, const char* driverName)
{
	GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(driverName);
	assert(driver != NULL);
	GDALDataset* src = (GDALDataset*)GDALOpen(filename, GA_ReadOnly);
	m_iRasterCnts = src->GetRasterCount();
	m_iRasterWidth = src->GetRasterXSize();
	m_iRasterHeight = src->GetRasterYSize();
	m_iSize = m_iRasterHeight*m_iRasterWidth*m_iRasterCnts;
	GDALDestroyDriver(driver);
	
}
void gdalData::getExtent(extent& extent)
{
	extent._width = m_iRasterWidth;
	extent._height = m_iRasterHeight;
	extent._channel = m_iRasterCnts;
}
void gdalData::getTile(BYTE* src, BYTE* dst, int row, int col, int N)
{
	assert(row < N && row>0);
	assert(col< N && col>0);
	assert(src != NULL&& dst != NULL);
	extent ext = getExtent();
	int channel = ext._channel;
	int srcWidth = ext._width;
	int tileWidth = (srcWidth - 1) / N + 1;
	memcpy(dst, src, tileWidth*tileWidth);

}
extent gdalData::getExtent()
{
	extent ext;
	getExtent(ext);
	return ext;
}

void gdalData::clip()
{
	//We only need a region whose size is (2^n+1)*(2^n+1)
	extent ext;
	getExtent(ext);
	int originalWidth = ext._width;
	int originalHeight = ext._height;
	float	m = log((float)originalWidth) / log(2.0f);
	float	n = log((float)originalHeight) / log(2.0f);

	int exp = ([](int a, int b){return a>b?b:a; })(m, n);

	int cliped = pow(2.0, exp) + 1;

	setSize(cliped, cliped);

}
void gdalData::setSize(int width, int height)
{
	m_iRasterWidth = width;
	m_iRasterHeight = height;
}
void gdalDataProxy::load(const char* filename, const char* driverName)
{
	m_driverName = driverName;
	m_filename = filename;
	std::unique_ptr<GDALDriver> tempDriver(GetGDALDriverManager()->GetDriverByName(driverName));
	m_driver= std::move(tempDriver);
	tempDriver.release();
	std::unique_ptr<gdalData> tempGdalData(new gdalData);
	m_gdalData = std::move(tempGdalData);
	tempGdalData.release();
	getInputData()->load(filename, driverName);
	getInputData()->clip();
}
void gdalDataProxy::getExtent(extent& extent)
{
	getInputData()->getExtent(extent);
}
void gdalDataProxy::getTile(BYTE* src, BYTE* dst, int row, int col, int N)
{
	getInputData()->getTile(src, dst, row, col, N);
}
void gdalDataProxy::getTile(BYTE* dst, int row, int col, int N)
{

	GDALDataset* src = (GDALDataset*)GDALOpen(getFilename(), GA_ReadOnly);
	assert(src != NULL);
	extent ext = getExtent();
	int tileWidth = (ext._width - 1) / N + 1;
	int tileHeight = (ext._height - 1) / N + 1;
	BYTE* buf = (BYTE*)CPLCalloc(tileWidth*tileHeight * 1 * sizeof(BYTE), 1);
	src->RasterIO(GF_Read, col*(tileWidth - 1), row*(tileHeight - 1), tileWidth, tileHeight, buf, tileWidth, tileHeight, GDT_Byte, 1, NULL, 1, 0, 1);

	getTile(buf, dst, row, col, N);

	GDALClose(src);
	CPLFree(buf);
}
extent gdalDataProxy::getExtent()
{
	return getInputData()->getExtent();
}
const char* gdalDataProxy::getFilename()
{
	return m_filename.c_str();
}
const char* gdalDataProxy::getDrivername()
{
	return m_driverName.c_str();
}

void gdalDataProxy::setSize(int width, int height)
{

	getInputData()->setSize(width, height);

}
void gdalDataProxy::clip()
{
	getInputData()->clip();
}
gdalData* gdalDataProxy::getInputData()
{
	return m_gdalData.get();
}
gdalDataProxy::gdalDataProxy() :dataImp()
{

}
dataImpFactory* dataImpFactory::instance()
{
	static dataImpFactory inst;
	return &inst;
}
dataImp* dataImpFactory::create(int type, const char* filename, const char* driverName)
{
	switch (type)
	{
	case _RAW:
		return createRawImp(filename);
	case _GDAL_SUPPORTED:
		return createGDALImp(filename, driverName);
	default:
		exit(0);
		break;
	}
}
dataImp* dataImpFactory::createRawImp(const char* filename)
{
	std::unique_ptr<rawDataProxy> raw(new rawDataProxy);
	raw->load(filename);
	return raw.release();
}
dataImp* dataImpFactory::createGDALImp(const char* filename, const char* driverName)
{

	assert(driverName != NULL);
	GDALAllRegister();
	std::unique_ptr<gdalDataProxy> gdal(new gdalDataProxy);
	gdal->load(filename, driverName);
	return gdal.release();
}
heightField::heightField(dataImp* input)
{
	setImp(input);
}
dataImp* heightField::getImp()
{
	return m_dataInputImp.get();
}

void heightField::setImp(dataImp* imp)
{
	std::unique_ptr<dataImp> temp(imp);

	m_dataInputImp = std::move(temp);

	temp.release();
}

int heightField::getWidth()
{
	extent ext = m_dataInputImp->getExtent();

	return ext._width;
}
int heightField::getHeight()
{
	extent ext = m_dataInputImp->getExtent();

	return ext._height;
}
int heightField::getChannel()
{
	extent ext = m_dataInputImp->getExtent();
	return ext._channel;
}
int heightField::getCenterX()
{
	int width = getWidth();
	return width >> 1;
}
int heightField::getCenterY()
{
	int height = getHeight();
	return height >> 1;
}

int heightField::getTileWidth(int i, int j, int N)
{
	int srcWidth = getWidth();
	int tileWidth = (srcWidth - 1) / N + 1;
	return tileWidth;
}
int heightField::getTileHeight(int i, int j, int N)
{
	int srcHeight = getHeight();
	int tileHeight = (srcHeight - 1) / N + 1;
	return tileHeight;
}

int heightField::getTileCenterX(int i, int j, int N)
{
	int tileWidth = getTileWidth(i, j, N);
	int tileWidthEven = tileWidth - 1;
	return i*tileWidthEven + tileWidth / 2;

}
int heightField::getTileCenterY(int i, int j, int N)
{
	int tileHeight = getTileHeight(i, j, N);
	int tileHeightEven = tileHeight - 1;
	return j*tileHeightEven + tileHeight / 2;
}
void heightField::generateTile(int i, int j, int N, BYTE* dst, Range& tileRange)
{

	dataImp* imp = getImp();

	imp->getTile(dst, i, j, N);
	tileRange._centerX = getTileCenterX(i, j, N);
	tileRange._centerY = getTileCenterY(i, j, N);
	tileRange._width = getTileWidth(i, j, N);
	tileRange._height = getTileHeight(i, j, N);
	tileRange._index_i = i;
	tileRange._index_j = j;
	tileRange._N = N;

}
