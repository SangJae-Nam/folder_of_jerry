/*   HANGULCMT   */
/**------------------------------------------------------------------------

�ѱ� �ּ� ����� - �ڷ�Ű!! ��

������ : �ܱ����б� MAZE 10�� ������

_2007/6/20_

edit : Hertz9th leejaeseong...

-------------------------------------------------------------------------*/
macro hangulcmt()
{
	sCmt = ask("�ּ� �Է��� �������~ ( for _varhae_ )");
	sCmt = cat("//" sCmt);

	hWnd = GetCurrentWnd()
	inFirst = GetWndSelLnFirst(hWnd)
	hBuf = GetCurrentBuf()
	sCurrent = GetBufLine(hBuf, inFirst)
	
	DelBufLine(hBuf, inFirst);  
	InsBufLine(hBuf, inFirst, "@sCurrent@ @sCmt@");	
}

