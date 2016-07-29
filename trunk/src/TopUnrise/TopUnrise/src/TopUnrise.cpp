#include "stdafx.h"
#include "plugin.h"
#include "stdio.h"

#include "MemLeaker.h"

#define MIN_SCALE 1
#define MAX_SCALE 10

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
    }
    return TRUE;
}

PDATAIOFUNC	 g_pFuncCallBack;

//获取回调函数
void RegisterDataInterface(PDATAIOFUNC pfn)
{
	g_pFuncCallBack = pfn;
}

//注册插件信息
void GetCopyRightInfo(LPPLUGIN info)
{
	//填写基本信息
	strcpy(info->Name,"上升空间筛选");
	strcpy(info->Dy,"US");
	strcpy(info->Author,"john");
	strcpy(info->Period,"短线");
	strcpy(info->Descript,"上升空间筛选");
	strcpy(info->OtherInfo,"自定义天数上升空间筛选");
	//填写参数信息
	info->ParamNum = 2;
	
	strcpy(info->ParamInfo[0].acParaName,"百分比");
	info->ParamInfo[0].nMin=0;
	info->ParamInfo[0].nMax=1000;
	info->ParamInfo[0].nDefault=20;

	strcpy(info->ParamInfo[1].acParaName,"扩大空间");
	info->ParamInfo[1].nMin=-0;
	info->ParamInfo[1].nMax=1;
	info->ParamInfo[1].nDefault=1;
}

////////////////////////////////////////////////////////////////////////////////
//自定义实现细节函数(可根据选股需要添加)

const	BYTE	g_nAvoidMask[]={0xF8,0xF8,0xF8,0xF8};	// 无效数据标志(系统定义)

char* g_nFatherCode[] = { "999999", "399001", "399005", "399006" };
int g_FatherUpPercent[] = {-1, -1, -1, -1};

typedef enum _eFatherCode
{
	EShangHaiZZ,
	EShenZhenCZ,
	EZhongXBZZ,
	EChuangYBZZ,
	EFatherCodeMax
} EFatherCode;

EFatherCode mathFatherCode(char* Code)
{
	if (NULL == Code)
		return EFatherCodeMax;

	EFatherCode eFCode = EFatherCodeMax;
	if (Code[0] == '6')
	{
		eFCode = EShangHaiZZ;
	}
	else if (Code[0] == '3')
	{
		eFCode = EChuangYBZZ;
	}
	else if (strstr(Code, "002") == Code)
	{
		eFCode = EZhongXBZZ;
	}
	else if (Code[0] == '0')
	{
		eFCode = EShenZhenCZ;
	}

	return eFCode;
}

LPHISDAT maxClose(LPHISDAT pHisDat, long lDataNum)
{
	if (NULL == pHisDat || lDataNum <= 0)
		return NULL;

	LPHISDAT pMax = pHisDat;
	for (long i = 0; i < lDataNum; i++)
	{
		if (pMax->Close < (pHisDat+i)->Close)
			pMax = pHisDat+i;
	}
	return pMax;
}


BOOL fEqual(float a, float b)
{
	const float fJudge = 0.01;
	float fValue = 0.0;

	if (a > b)
		fValue = a - b;
	else 
		fValue = b - a;

	if (fValue > fJudge)
		return FALSE;

	return TRUE;
}


BOOL dateEqual(NTime t1, NTime t2)
{
	if (t1.year != t2.year || t1.month != t2.month || t1.day != t2.day)
		return FALSE;

	return TRUE;
}


NTime dateInterval(NTime nLeft, NTime nRight)
{
	NTime nInterval;
	memset(&nInterval, 0, sizeof(NTime));
	
	unsigned int iLeft = 0;
	unsigned int iRight = 0;
	unsigned int iInterval = 0;

	const unsigned int cDayofyear = 365;
	const unsigned int cDayofmonth = 30;

	iLeft = nLeft.year*cDayofyear + nLeft.month*cDayofmonth + nLeft.day;
	iRight = nRight.year*cDayofyear + nRight.month*cDayofmonth + nRight.day;

	iInterval = (iLeft > iRight) ? iLeft - iRight : iRight - iLeft;

	nInterval.year = iInterval / cDayofyear;
	iInterval = iInterval % cDayofyear;
	nInterval.month = iInterval / cDayofmonth;
	iInterval = iInterval % cDayofmonth;
	nInterval.day = iInterval;

	return nInterval;
}


/* 过滤函数
   返回值：以S和*开头的股票 或者 上市不满一年，返回FALSE，否则返回TRUE
*/
BOOL filterStock(char * Code, short nSetCode, NTime time1, NTime time2, BYTE nTQ)
{
	if (NULL == Code)
		return FALSE;

	const unsigned short cMinYears = 2;	
	const short cInfoNum = 2;
	short iInfoNum = cInfoNum;

	{
		STOCKINFO stockInfoArray[cInfoNum];
		memset(stockInfoArray, 0, cInfoNum*sizeof(STOCKINFO));

		LPSTOCKINFO pStockInfo = stockInfoArray;

		//获取股票名称以及上市时间
		long readnum = g_pFuncCallBack(Code, nSetCode, STKINFO_DAT, pStockInfo, iInfoNum, time1, time2, nTQ, 0);
		if (readnum <= 0)
		{
			//delete[] pStockInfo;
			pStockInfo = NULL;
			return FALSE;
		}
		if ('S' == pStockInfo->Name[0] || '*' == pStockInfo->Name[0])
		{
			//delete[] pStockInfo;
			pStockInfo = NULL;
			return FALSE;
		}

		NTime startDate, todayDate, dInterval;
		memset(&startDate, 0, sizeof(NTime));
		memset(&todayDate, 0, sizeof(NTime));
		memset(&dInterval, 0, sizeof(NTime));

		long lStartDate = pStockInfo->J_start;
		startDate.year = lStartDate / 10000;
		lStartDate = lStartDate % 10000;
		startDate.month = lStartDate / 100;
		lStartDate = lStartDate % 100;
		startDate.day = lStartDate;

		//获取今天日期
		SYSTEMTIME tdTime;
		memset(&tdTime, 0, sizeof(SYSTEMTIME));
		GetLocalTime(&tdTime);

		todayDate.year = tdTime.wYear;
		todayDate.month = tdTime.wMonth;
		todayDate.day = tdTime.wDay;

		dInterval = dateInterval(startDate, todayDate);

		//太年轻的股返回FALSE
		if (dInterval.year < cMinYears)
		{
			//delete[] pStockInfo;
			pStockInfo = NULL;
			return FALSE;
		}

		//delete[] pStockInfo;
		pStockInfo = NULL;
	}
	
	{
		//获取股票当天的信息，当天停牌的返回FALSE
		REPORTDAT2 reportArray[cInfoNum];
		memset(reportArray, 0, cInfoNum*sizeof(REPORTDAT2));

		LPREPORTDAT2 pReportDat2 = reportArray;
		
		//获取股票当天开盘信息
		long readnum = g_pFuncCallBack(Code, nSetCode, REPORT_DAT2, pReportDat2, iInfoNum, time1, time2, nTQ, 0);
		if (0 >= readnum)
		{
			pReportDat2 = NULL;
			return FALSE;
		}

		if ( fEqual(pReportDat2->Open, 0) )
		{
			pReportDat2 = NULL;
			return FALSE;
		}
		pReportDat2 = NULL;
	}	

	return TRUE;
}


/* 计算上升空间 */
int calcUpPercent(char * Code, short nSetCode, short DataType, NTime time1, NTime time2, BYTE nTQ)
{
	int iUpSpace = -1;

	LPHISDAT pMax = NULL;

	//窥视数据个数
	long datanum = g_pFuncCallBack(Code, nSetCode, DataType, NULL, -1, time1, time2, nTQ, 0);
	if ( 1 > datanum ){
		return iUpSpace;
	}

	LPHISDAT pHisDat = new HISDAT[datanum];

	long readnum = g_pFuncCallBack(Code, nSetCode, DataType, pHisDat, datanum, time1, time2, nTQ, 0);
	if ( 1 > readnum || readnum > datanum )
	{
		OutputDebugStringA("========= g_pFuncCallBack read error! =========\n");
		delete[] pHisDat;
		pHisDat = NULL;
		return iUpSpace;
	}

	//停牌股不计算直接返回
	LPHISDAT pLate = pHisDat + readnum - 1;

	//查找最高收盘价
	pMax = maxClose(pHisDat, readnum);

	if (NULL == pMax)
	{
		OutputDebugStringA("========= maxClose error! =========\n");
		delete[] pHisDat;
		pHisDat=NULL;
		return iUpSpace;
	}
	/*计算空间百分比*/

	iUpSpace = int(((pMax->Close - pLate->Close)/pLate->Close) * 100);

	delete[] pHisDat;
	pHisDat=NULL;

	return iUpSpace;
}


BOOL InputInfoThenCalc1(char * Code,short nSetCode,int Value[4],short DataType,short nDataNum,BYTE nTQ,unsigned long unused) //按最近数据计算
{
	BOOL nRet = FALSE;
	return nRet;
}

BOOL InputInfoThenCalc2(char * Code,short nSetCode,int Value[4],short DataType,NTime time1,NTime time2,BYTE nTQ,unsigned long unused)  //选取区段
{
	BOOL nRet = FALSE;

	if ( (Value[0] < 0 || Value[0] > 1000) 
		|| (Value[1] != 0 && Value[1] != 1)  
		|| NULL == Code )
		goto endCalc2;	

	int iFatherRate = 0, iSonRate = 0;

	/* 计算对应大盘上升空间百分比 */
	/*EFatherCode eFCode = mathFatherCode(Code);
	if (EFatherCodeMax == eFCode)
	{
		OutputDebugStringA("========= Didn't find FatherCode for ");
		OutputDebugStringA(Code);
		OutputDebugStringA(" =========\n");
		goto endCalc2;
	}*/
	//判断是否计算过对应指数
	//if (g_FatherUpPercent[eFCode] < 0)
	//{
	//	/**读取对应大盘数据 */
	//	iFatherRate = calcUpPercent(g_nFatherCode[eFCode], nSetCode, DataType, time1, time2, nTQ);
	//	if ( iFatherRate < 0 )
	//	{
	//		OutputDebugStringA("=========== father calcUpPercent error!!\n");
	//		goto endCalc2;
	//	}
	//	g_FatherUpPercent[eFCode] = iFatherRate;
	//}
	//else
	//{
	//	iFatherRate = g_FatherUpPercent[eFCode];
	//}		

	/* 过滤垃圾股和停牌股 */
	if (FALSE == filterStock(Code, nSetCode, time1, time2, nTQ))
	{
		OutputDebugStringA("===== filter stock : ");
		OutputDebugStringA(Code);
		OutputDebugStringA(" =========\n");
		goto endCalc2;
	}

	/* 计算个股上升空间百分比 */
	iSonRate = calcUpPercent(Code, nSetCode, DataType, time1, time2, nTQ);
	if (iSonRate < 0)
	{
		OutputDebugStringA("=========== son calcUpPercent error!!\n");
		goto endCalc2;
	}

	if (0 == Value[1])
	{
		if (iSonRate <= Value[0])
			nRet = TRUE;
	} else
	{
		if (iSonRate >= Value[0])
			nRet = TRUE;
	}
	
endCalc2:
	MEMLEAK_OUTPUT();

	return nRet;
}
