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

//��ȡ�ص�����
void RegisterDataInterface(PDATAIOFUNC pfn)
{
	g_pFuncCallBack = pfn;
}

//ע������Ϣ
void GetCopyRightInfo(LPPLUGIN info)
{
	//��д������Ϣ
	strcpy(info->Name,"�����ռ�ɸѡ");
	strcpy(info->Dy,"US");
	strcpy(info->Author,"john");
	strcpy(info->Period,"����");
	strcpy(info->Descript,"�����ռ�ɸѡ");
	strcpy(info->OtherInfo,"�Զ������������ռ�ɸѡ");
	//��д������Ϣ
	info->ParamNum = 2;
	
	strcpy(info->ParamInfo[0].acParaName,"�ٷֱ�");
	info->ParamInfo[0].nMin=0;
	info->ParamInfo[0].nMax=1000;
	info->ParamInfo[0].nDefault=20;

	strcpy(info->ParamInfo[1].acParaName,"����ռ�");
	info->ParamInfo[1].nMin=-0;
	info->ParamInfo[1].nMax=1;
	info->ParamInfo[1].nDefault=1;
}

////////////////////////////////////////////////////////////////////////////////
//�Զ���ʵ��ϸ�ں���(�ɸ���ѡ����Ҫ���)

const	BYTE	g_nAvoidMask[]={0xF8,0xF8,0xF8,0xF8};	// ��Ч���ݱ�־(ϵͳ����)

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


/* ���˺���
   ����ֵ����S��*��ͷ�Ĺ�Ʊ ���� ���в���һ�꣬����FALSE�����򷵻�TRUE
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

		//��ȡ��Ʊ�����Լ�����ʱ��
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

		//��ȡ��������
		SYSTEMTIME tdTime;
		memset(&tdTime, 0, sizeof(SYSTEMTIME));
		GetLocalTime(&tdTime);

		todayDate.year = tdTime.wYear;
		todayDate.month = tdTime.wMonth;
		todayDate.day = tdTime.wDay;

		dInterval = dateInterval(startDate, todayDate);

		//̫����Ĺɷ���FALSE
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
		//��ȡ��Ʊ�������Ϣ������ͣ�Ƶķ���FALSE
		REPORTDAT2 reportArray[cInfoNum];
		memset(reportArray, 0, cInfoNum*sizeof(REPORTDAT2));

		LPREPORTDAT2 pReportDat2 = reportArray;
		
		//��ȡ��Ʊ���쿪����Ϣ
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


/* ���������ռ� */
int calcUpPercent(char * Code, short nSetCode, short DataType, NTime time1, NTime time2, BYTE nTQ)
{
	int iUpSpace = -1;

	LPHISDAT pMax = NULL;

	//�������ݸ���
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

	//ͣ�ƹɲ�����ֱ�ӷ���
	LPHISDAT pLate = pHisDat + readnum - 1;

	//����������̼�
	pMax = maxClose(pHisDat, readnum);

	if (NULL == pMax)
	{
		OutputDebugStringA("========= maxClose error! =========\n");
		delete[] pHisDat;
		pHisDat=NULL;
		return iUpSpace;
	}
	/*����ռ�ٷֱ�*/

	iUpSpace = int(((pMax->Close - pLate->Close)/pLate->Close) * 100);

	delete[] pHisDat;
	pHisDat=NULL;

	return iUpSpace;
}


BOOL InputInfoThenCalc1(char * Code,short nSetCode,int Value[4],short DataType,short nDataNum,BYTE nTQ,unsigned long unused) //��������ݼ���
{
	BOOL nRet = FALSE;
	return nRet;
}

BOOL InputInfoThenCalc2(char * Code,short nSetCode,int Value[4],short DataType,NTime time1,NTime time2,BYTE nTQ,unsigned long unused)  //ѡȡ����
{
	BOOL nRet = FALSE;

	if ( (Value[0] < 0 || Value[0] > 1000) 
		|| (Value[1] != 0 && Value[1] != 1)  
		|| NULL == Code )
		goto endCalc2;	

	int iFatherRate = 0, iSonRate = 0;

	/* �����Ӧ���������ռ�ٷֱ� */
	/*EFatherCode eFCode = mathFatherCode(Code);
	if (EFatherCodeMax == eFCode)
	{
		OutputDebugStringA("========= Didn't find FatherCode for ");
		OutputDebugStringA(Code);
		OutputDebugStringA(" =========\n");
		goto endCalc2;
	}*/
	//�ж��Ƿ�������Ӧָ��
	//if (g_FatherUpPercent[eFCode] < 0)
	//{
	//	/**��ȡ��Ӧ�������� */
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

	/* ���������ɺ�ͣ�ƹ� */
	if (FALSE == filterStock(Code, nSetCode, time1, time2, nTQ))
	{
		OutputDebugStringA("===== filter stock : ");
		OutputDebugStringA(Code);
		OutputDebugStringA(" =========\n");
		goto endCalc2;
	}

	/* ������������ռ�ٷֱ� */
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
