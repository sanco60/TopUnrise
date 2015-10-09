// choice1.cpp : Defines the entry point for the DLL application.
// ���ʵ��

#include "stdafx.h"
#include "plugin.h"

#include "MemLeaker.h"

#define PLUGIN_EXPORTS

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
	info->ParamNum = 1;
	strcpy(info->ParamInfo[0].acParaName,"����");
	info->ParamInfo[0].nMin=MIN_SCALE;
	info->ParamInfo[0].nMax=MAX_SCALE;
	info->ParamInfo[0].nDefault=2;
}

////////////////////////////////////////////////////////////////////////////////
//�Զ���ʵ��ϸ�ں���(�ɸ���ѡ����Ҫ���)

const	BYTE	g_nAvoidMask[]={0xF8,0xF8,0xF8,0xF8};	// ��Ч���ݱ�־(ϵͳ����)

char* g_nFatherCode[] = { "999999", "399001", "399005", "399006" };
float g_fFatherRate[] = {0.0, 0.0, 0.0, 0.0};

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
	float fJudge = 0.01;
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


BOOL InputInfoThenCalc1(char * Code,short nSetCode,int Value[4],short DataType,short nDataNum,BYTE nTQ,unsigned long unused) //��������ݼ���
{
	BOOL nRet = FALSE;
	return nRet;
}

BOOL InputInfoThenCalc2(char * Code,short nSetCode,int Value[4],short DataType,NTime time1,NTime time2,BYTE nTQ,unsigned long unused)  //ѡȡ����
{
	BOOL nRet = FALSE;

	if ( Value[0] < MIN_SCALE || Value[0] > MAX_SCALE 
		|| NULL == Code )
		goto endCalc2;	

	float fFatherRate = 0.0, fSonRate = 0.0;

	/* �����Ӧ���������ռ�ٷֱ� */
	{
		EFatherCode eFCode = mathFatherCode(Code);
		if (EFatherCodeMax == eFCode)
		{
			OutputDebugString(L"========= Didn't find FatherCode for ");
			OutputDebugString((LPCWSTR)Code);
			OutputDebugString(L" =========\n");
			goto endCalc2;
		}
		//�ж��Ƿ�������Ӧָ��
		if (fEqual(g_fFatherRate[eFCode], 0.0))
		{
			/**��ȡ��Ӧ�������� */
			LPHISDAT pMax = NULL;

			//�������ݸ���
			long datanum = g_pFuncCallBack(g_nFatherCode[eFCode],nSetCode,DataType,NULL,-1,time1,time2,nTQ,0);
			if ( 2 > datanum )
			{
				//OutputDebugString(L"========= g_pFuncCallBack error! =========\n");
				goto endCalc2;
			}
		
			LPHISDAT pHisDat = new HISDAT[datanum];

			long readnum = g_pFuncCallBack(g_nFatherCode[eFCode],nSetCode,DataType,pHisDat,datanum,time1,time2,nTQ,0);
			if ( 2 > readnum || readnum > datanum )
			{
				OutputDebugString(L"========= g_pFuncCallBack read error! =========\n");
				delete[] pHisDat;
				pHisDat = NULL;
				goto endCalc2;
			}
		
			//����������̼�
			pMax = maxClose(pHisDat, readnum);
			if (NULL == pMax)
			{
				OutputDebugString(L"========= maxClose error! =========\n");
				delete[] pHisDat;
				pHisDat = NULL;
				goto endCalc2;
			}
			//����ռ�ٷֱ�
			LPHISDAT pLate = pHisDat + readnum - 1;
			fFatherRate = (pMax->Close - pLate->Close)/pLate->Close;

			g_fFatherRate[eFCode] = fFatherRate;

			delete[] pHisDat;
			pHisDat = NULL;
		}
		else
		{
			fFatherRate = g_fFatherRate[eFCode];
		}		
	}

	/* �������ƿ�ͷΪ��S��*�Ĺ�Ʊ */
	{
		LPSTOCKINFO pStockInfo = new STOCKINFO[2];
		memset(pStockInfo, 0, 2*sizeof(STOCKINFO));
		long readnum = g_pFuncCallBack(Code, nSetCode, STKINFO_DAT, pStockInfo, 1, time2, time2, nTQ, 0);
		if (readnum <= 0)
		{
			delete[] pStockInfo;
			pStockInfo = NULL;
			goto endCalc2;
		}
		if ('S' == pStockInfo->Name[0] || '*' == pStockInfo->Name[0])
		{
			delete[] pStockInfo;
			pStockInfo = NULL;
			goto endCalc2;
		}
		delete[] pStockInfo;
		pStockInfo = NULL;
	}

	/* ������������ռ�ٷֱ� */
	{
		/**��ȡ����ȡ���� */
		LPHISDAT pMax = NULL;

		//�������ݸ���
	    long datanum = g_pFuncCallBack(Code,nSetCode,DataType,NULL,-1,time1,time2,nTQ,0);
		if ( 2 > datanum )
		{
			//OutputDebugString(L"========= g_pFuncCallBack error! =========\n");
			goto endCalc2;
		}

		LPHISDAT pHisDat = new HISDAT[datanum];

		long readnum = g_pFuncCallBack(Code,nSetCode,DataType,pHisDat,datanum,time1,time2,nTQ,0);
		if ( 2 > readnum || readnum > datanum )
		{
			OutputDebugString(L"========= g_pFuncCallBack read error! =========\n");
			delete[] pHisDat;
			pHisDat = NULL;
			goto endCalc2;
		}
		
		//����������̼�
		pMax = maxClose(pHisDat, readnum);
		if (NULL == pMax)
		{
			OutputDebugString(L"========= maxClose error! =========\n");
			delete[] pHisDat;
			pHisDat=NULL;
			goto endCalc2;
		}
		/*����ռ�ٷֱ�*/
		LPHISDAT pLate = pHisDat + readnum - 1;
		//ͣ�ƵĹ�Ʊ����
		/*if (!dateEqual(pLate->Time, time2))
		{
			OutputDebugString(L"========= ");
			OutputDebugString((LPCWSTR)Code);
			OutputDebugString(L" Stop Trading.=========\n");

			delete[] pHisDat;
			pHisDat=NULL;
			goto endCalc2;
		}*/

		fSonRate = (pMax->Close - pLate->Close)/pLate->Close;
		delete[] pHisDat;
		pHisDat=NULL;
	}
	
	if (fSonRate > fFatherRate*((float)Value[0]))
		nRet = TRUE;
	
endCalc2:
	MEMLEAK_OUTPUT();
	return nRet;
}
