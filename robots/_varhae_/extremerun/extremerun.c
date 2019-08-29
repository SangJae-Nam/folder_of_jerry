//###########################################################################
//
// FILE		: extremerun.c
//
// TITLE		: _varhae_ Tracer extreme run source file.
//
// Author	: leejaeseong
//
// Company	: Hertz
//
//###########################################################################
// $Release Date: 2009.12.24 $
//###########################################################################

#include "DSP2833x_Device.h"		// DSP2833x Headerfile Include File
#include "DSP2833x_Examples.h"		// DSP2833x Examples Include File

typedef enum extrem_variable_vel
{
	LIMIT_45_VEL = 300 , 
	LIMIT_90_VEL = 200 , 

	LIMIT_SHIFT_VEL = 2800 ,

	LIMIT_ACC = 10

}x_vari_vel_e;

#define SHIFT_RATIO			( float32 )6.0
#define RETURN_RATIO		( float32 )7.0
#define ST_RET_RATIO		( float32 )2.4
#define PM_RATIO			( float32 )2.4

volatile float32 right_table[] = 
{
	0.0 , 1000.0 , 2000.0 , 3000.0 , 4275.0 , 6500.0 , 7200.0 , 8000.0 , 10000.0 , 12000.0 , 14000.0
};

volatile float32 left_table[] = 
{
	0.0 , -1000.0 , -2000.0 , -3000.0 , -4275.0 , -6500.0 , -7200.0 , -8000.0 , -10000.0 , -12000.0 , -14000.0
};

static void xcontinus_angle_vel_compute_func( sec_info_t *p_info , volatile float32 dist , volatile float32 kp );
static void xmemmove_sec_info_struct_func( sec_info_t *p_cur , sec_info_t *p_next , volatile float32 limit_vel , volatile float32 m_dist );
static void xsecession_for_angle_func( sec_info_t *pinfo , err_dps_t *perr , int32 mark_cnt );
static void xpos_shift_func( volatile float32 cur_dist , volatile float32 shift_dist , sec_info_t *p_info  );


static void xstraight_compute( sec_info_t *p_info , err_dps_t *p_err , int32 mark_cnt ) //���� �϶�
{
	int32 shift = g_int32shift_level;

	sec_info_t *pinfo = p_info;
	err_dps_t *perr = p_err;

	volatile float32 big_vel = 0.0;	
	volatile float32 small_vel = 0.0;

	pinfo->fp32kp_down = POS_KP_UP;

	if(  mark_cnt > 0 )
	{
		pinfo->fp32in_vel  = ( pinfo - 1 )->fp32out_vel ? ( pinfo - 1 )->fp32out_vel : ( float32 )( g_int32turn_vel );
	}
	else
		pinfo->fp32in_vel = ( float32 )( 0.0 );

	if( !( pinfo->int32dir & DIR_END ) )
	{
		xsecession_for_angle_func( ( pinfo + 1 ) , perr , ( mark_cnt + 1 ) );  //���� �� �̸� ��� �� �Ŀ� out_vel �� ����
		pinfo->fp32out_vel = ( pinfo + 1 )->fp32in_vel;

		if( pinfo->fp32out_vel == 0.0 ) pinfo->fp32out_vel = ( float32 )( g_int32turn_vel );		
	}
	else
		pinfo->fp32out_vel = 0.0;	
	
	do
	{
		pinfo->int32down_flag = OFF;
		pinfo->int32s44s_flag = OFF;
	
		if( pinfo->int32dist > MAX_DIST_LIMIT )  //�� ����
		{
			pinfo->fp32shift_before = 0.0; //������ ���ư�.
						
			pinfo->int32acc = g_int32max_acc;
			pinfo->fp32dist_limit = ( float32 )( pinfo->int32dist ) * 0.65;  //�� �Ÿ��� 65% ������ ����Ʈ ����
		}
		else if( pinfo->int32dist > MID_DIST_LIMIT )
		{
			pinfo->fp32shift_before = 0.0; //������ ���ư�.
						
			pinfo->int32acc = g_int32mid_acc;
			pinfo->fp32dist_limit = ( float32 )( pinfo->int32dist ) * 0.4;	//�� �Ÿ��� 40% ������ ����Ʈ ����
		}
		else
		{
			if( mark_cnt > 0 )
				pinfo->fp32shift_before = ( ( pinfo - 1 )->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];
			else
				pinfo->fp32shift_before = 0.0;  //start!!
					
			pinfo->int32acc = g_int32short_acc;
			pinfo->fp32dist_limit = ( float32 )( pinfo->int32dist ) * 0.15;  //�� �Ÿ��� 15% ������ ����Ʈ ����
		}
				
		if( pinfo->int32dir & DIR_END ) //END �̸� ���ǹ� �ʿ� ����...
			break;		

		pinfo->fp32shift_after = ( ( pinfo + 1 )->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];		
		
		if( ( ( pinfo + 1 )->int32dir & TURN_45 ) && ( ( pinfo + 2 )->int32dir & ( TURN_45 | TURN_90 ) ) ) // ������ | ���� - 45�� - 45�� - ���� | ���� - 45�� - 90�� - 45��
		{
			pinfo->fp32kp_down = POS_KP_DOWN;				
			
			if( pinfo->int32dist < ARBITRATE )	pinfo->int32down_flag = ON;
			else 								pinfo->int32s44s_flag = ON;

			if( ( pinfo + 2 )->int32dir & TURN_45 )
				pinfo->fp32shift_after = ( ( pinfo + 1 )->int32dir & RIGHT_TURN ) ? right_table[ shift + 1 ] : left_table[ shift + 1 ];
		}
		
	}
	while( 0 );

	if( pinfo->int32dir & DIR_END )   //END�϶� ������ ����� ����...
	{
		pinfo->fp32shift_after = 0.0;
		pinfo->fp32dist_limit = ( float32 )( pinfo->int32dist >> 1 );
	}

	if( !mark_cnt && pinfo->int32acc > 10 )	pinfo->int32acc = 10;  //���� ���� ���ӵ� ����...

	big_vel = MAX( pinfo->fp32in_vel , pinfo->fp32out_vel );
	small_vel = MIN( pinfo->fp32in_vel , pinfo->fp32out_vel );
	diffvel_to_remaindist_cpt( pinfo->fp32in_vel , pinfo->fp32out_vel , pinfo->int32acc , &pinfo->fp32m_dist );  //�ӵ� �ٸ� ���� �Ÿ��� ���

	if( pinfo->fp32m_dist >= ( float32 )( pinfo->int32dist ) )  //������ ������ ���� �Ÿ����� Ŭ ��� -> ���� �ʿ�!!!
	{
		pinfo->fp32decel_dist = ( float32 )( pinfo->int32dist );
		dist_to_maxvel_cpt( ( float32 )( pinfo->int32dist ) , ( float32 )0.0 , small_vel ,  pinfo->int32acc , &( pinfo->fp32vel ) ); //���� �Ÿ��� ��ӵ����� �ְ� �ӵ��� ���			

		if( pinfo->fp32in_vel > pinfo->fp32out_vel )	pinfo->fp32in_vel = pinfo->fp32vel;
		else											pinfo->fp32out_vel = pinfo->fp32vel;	

		if( !mark_cnt )  //start
			pinfo->fp32in_vel = 0.0;
	}
	else
	{
		dist_to_maxvel_cpt( ( float32 )( pinfo->int32dist ) , pinfo->fp32m_dist , big_vel ,  pinfo->int32acc , &( pinfo->fp32vel ) ); //���� �Ÿ��� ��ӵ����� �ְ� �ӵ��� ���	
		diffvel_to_remaindist_cpt( pinfo->fp32vel , pinfo->fp32out_vel , pinfo->int32acc , &( pinfo->fp32decel_dist ) );   //���� �Ÿ� ���	
	}


	perr->fp32err_dist[ mark_cnt ] = ( float32 )( pinfo->int32dist << 2 );  //�Ÿ� ���� üũ ��

	if( perr->fp32err_dist[ mark_cnt ] > ( float32 )( MID_DIST_LIMIT + SHT_DIST_LIMIT ) )  //���� �Ÿ��� �� ��� -> �����Ÿ�  �� ����.
		perr->fp32err_dist[ mark_cnt ] = ( float32 )( MID_DIST_LIMIT + SHT_DIST_LIMIT ); 

	perr->fp32err_dist[ mark_cnt ] += ( float32 )( pinfo->int32dist );  //������� �������� �����ش�.

	perr->fp32under_dist[ mark_cnt ] = ( float32 )( pinfo->int32dist ) * 0.65;  //��ũ üũ ���� �Ÿ� ����.
	
}

static void x45angle_turn_compute( sec_info_t *p_info , err_dps_t *perr , int32 mark_cnt )  // 45�� ���
{
	int32 shift = g_int32shift_level;
	sec_info_t *pinfo = p_info;
	volatile float32 m_dist = 0.0;
	
	xsecession_for_angle_func( ( pinfo + 1 ) , perr , ( mark_cnt + 1 ) );		

	pinfo->fp32kp_down = POS_KP_UP;	
	pinfo->int32acc = g_int32turn_acc;

	if( ( ( pinfo - 1 )->int32dir & STRAIGHT ) && ( ( pinfo + 1 )->int32dir & STRAIGHT ) )  //���� - 45�� - ����
	{
		pinfo->int32acc = LIMIT_ACC;	
		xmemmove_sec_info_struct_func( pinfo , pinfo + 1 , ( float32 )( g_int32s4s_vel ) , m_dist );  //2800�� MAX �ӵ��� -> �׽�Ʈ ���� ��... ���� ����	
		pinfo->fp32shift_before = ( pinfo->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];
	}
	else
	{
		pinfo->fp32in_vel = ( float32 )( g_int32turn_vel );
		pinfo->fp32kp_down = POS_KP_DOWN;		

		do
		{
			if( ( mark_cnt < 2 ) || ( ( pinfo + 1 )->int32dir & DIR_END ) ) //array index < -1 || array index > g_int32total_mark -> ���� ó��,,,
			{
				pinfo->fp32kp_down = POS_KP_UP;
			
				pinfo->fp32vel = pinfo->fp32out_vel = pinfo->fp32in_vel = ( float32 )( g_int32turn_vel );

				//���� �ٷ� Ʋ��!!
				if( ( pinfo + 1 )->int32dir & STRAIGHT )
					pinfo->fp32shift_before = ( pinfo->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];
				else		
					pinfo->fp32shift_before = ( ( pinfo + 1 )->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];
			
				break;
			}

			m_dist = ( pinfo + 1 )->int32dist > MID_DIST_LIMIT ? ( float32 )( ( pinfo + 1 )->int32dist >> 2 ) : ( float32 )( ( pinfo + 1 )->int32dist >> 1 );
		
			if( ( ( pinfo - 1 )->int32dir & STRAIGHT ) && ( ( pinfo + 1 )->int32dir & TURN_45 ) && ( ( pinfo + 2 )->int32dir & STRAIGHT ) ) //���� - 45�� - 45�� - ���� ������ 45��,,,
			{
				pinfo->int32down_flag = ON;

				xmemmove_sec_info_struct_func( pinfo , pinfo + 2 , ( float32 )( g_int32s44s_vel ) , m_dist );  //Ż�� ������ ������ -> pinfo + 2
			
				//���� ����.
				pinfo->fp32shift_before = ( ( pinfo + 1 )->int32dir & RIGHT_TURN ) ? right_table[ shift + 1 ] : left_table[ shift + 1 ];  //���� �� ������ �������� ����Ʈ...
			}
			else if( ( ( pinfo - 2 )->int32dir & STRAIGHT ) && ( ( pinfo - 1 )->int32dir & TURN_45 ) && ( ( pinfo + 1 )->int32dir & STRAIGHT ) )
			{
				pinfo->int32down_flag = ON;
				pinfo->int32escape_flag = ON;	//escape �̸� 90�� ���� �ϸ� �ȵǹǷ�...				

				xmemmove_sec_info_struct_func( pinfo , pinfo + 1 , ( float32 )( g_int32s44s_vel ) , m_dist );  //Ż�� ������ ������ -> pinfo + 1
			
				//���� ����.
				pinfo->fp32shift_before = ( pinfo->int32dir & RIGHT_TURN ) ? right_table[ shift + 1 ] : left_table[ shift + 1 ];  //���� �� ������ �������� ����Ʈ...
			}	
			else if( ( ( pinfo - 1  )->int32dir > TURN_180 ) && ( ( pinfo + 1 )->int32dir > TURN_180 ) )  //ū�� - 45�� - ū�� �϶� 45������ ũ�� Ʋ�� �ʵ���...
			{
				pinfo->int32down_flag = ON;
				pinfo->fp32vel = pinfo->fp32out_vel = pinfo->fp32in_vel;					
			}
			else if( ( pinfo + 1 )->int32dir & ( TURN_45 | TURN_90 ) )  //���� ��
			{
				xcontinus_angle_vel_compute_func( pinfo , ( float32 )( pinfo->int32dist >> 1 ) , POS_KP_DOWN );

				if( ( pinfo + 1 )->int32dir & TURN_90 )
					pinfo->fp32shift_before = ( ( pinfo + 1 )->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ]; 
			}
			else if( ( ( pinfo - 1 )->int32dir & TURN_45 ) && ( ( pinfo + 1 )->int32dir & STRAIGHT ) ) //45�� ������ Ż�� - ���� 
			{
				pinfo->int32down_flag = ON;		//���� Ǯ�鼭 ���� �������� ���� -> ������ �������� ���� �ǹǷ�...
				pinfo->int32escape_flag = ON;	//escape �̸� 90�� ���� �ϸ� �ȵǹǷ�...

				xmemmove_sec_info_struct_func( pinfo , pinfo + 1 , ( float32 )( g_int32escape45_vel ) , m_dist );				

				if( ( pinfo + 1 )->int32dist > MID_DIST_LIMIT )
					pinfo->fp32shift_before = 0.0;  //�Ÿ��� �涧 ������ ��鸲 ����,,,
				else
					pinfo->fp32shift_before = ( pinfo->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];
			}
			else if( ( ( pinfo - 2 )->int32dir & ( TURN_45 | TURN_90 ) ) && ( ( pinfo  - 1 )->int32dir & TURN_90 ) && ( ( pinfo + 1 )->int32dir & STRAIGHT ) ) // ( 45�� | 90�� ) - 90�� - 45�� - ���� ������ Ż�� 45��...
			{
				pinfo->int32down_flag = ON;  	//���� Ǯ�鼭 ���� �������� ���� -> ������ �������� ���� �ǹǷ�...
				pinfo->int32escape_flag = ON;	//escape �̸� 90�� ���� �ϸ� �ȵǹǷ�...				

				xmemmove_sec_info_struct_func( pinfo , pinfo + 1 , ( float32 )( g_int32escape45_vel ) , m_dist );

				pinfo->fp32shift_before = ( pinfo->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];  //������ �����̹Ƿ� ���� �������� ����Ʈ.		
			}
			else
			{
				pinfo->fp32kp_down = POS_KP_UP;
			
				//���� �� -> ���� �� �ӵ� ���� 300 �� ���δ�!!
				if( ( pinfo + 1 )->int32dir & STRAIGHT )
					xmemmove_sec_info_struct_func( pinfo , pinfo + 1 , ( float32 )( g_int32turn_vel + LIMIT_45_VEL ) , m_dist );
				else
				{
					dist_to_maxvel_cpt( ( float32 )( pinfo->int32dist ) , ( float32 )( pinfo->int32dist >> 1 ) , ( float32 )( g_int32turn_vel ) ,  pinfo->int32acc , &pinfo->fp32vel );

					if( pinfo->fp32vel > ( float32 )( g_int32turn_vel + LIMIT_45_VEL ) )
						pinfo->fp32vel = ( float32 )( g_int32turn_vel + LIMIT_45_VEL );
					
					diffvel_to_remaindist_cpt( pinfo->fp32vel , ( float32 )( g_int32turn_vel ) , pinfo->int32acc , &pinfo->fp32decel_dist );

					pinfo->fp32in_vel = pinfo->fp32out_vel = ( float32 )g_int32turn_vel;
				}

				//���� �ٷ� Ʋ��!!
				if( ( pinfo + 1 )->int32dir & STRAIGHT )
					pinfo->fp32shift_before = ( pinfo->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];
				else		
					pinfo->fp32shift_before = ( ( pinfo + 1 )->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];
			}
		
		}
		while( 0 );
		
	}	

	pinfo->fp32shift_after = pinfo->fp32shift_before;	
	pinfo->fp32dist_limit = ( float32 )( pinfo->int32dist >> 1 );					

	perr->fp32err_dist[ mark_cnt ] = ( float32 )( pinfo->int32dist << 1 ); 		//������� �������� �����ش�.
	perr->fp32under_dist[ mark_cnt ] = ( float32 )( pinfo->int32dist >> 2 );	//��ũ üũ ���� �Ÿ� ����.
	
}

static void x90angle_turn_compute( sec_info_t *p_info , err_dps_t *perr , int32 mark_cnt )  // 90�� ���
{
	sec_info_t *pinfo = p_info;
	volatile float32 m_dist = 0.0;
	int32 shift = g_int32shift_level;	

	int32 ret = 0;

	pinfo->int32acc = g_int32turn_acc;

	pinfo->fp32kp_down = POS_KP_UP;	

	pinfo->fp32in_vel = ( float32 )( g_int32turn_vel );	
	pinfo->fp32vel = pinfo->fp32out_vel = pinfo->fp32in_vel;

	pinfo->fp32shift_before = ( pinfo->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];
	
	if( ( pinfo + 1 )->int32dir & STRAIGHT )
	{	
		do
		{
			if( ( mark_cnt > 2 && ( pinfo - 2 )->int32escape_flag == ON ) ||
				( ( pinfo - 1 )->int32dir & STRAIGHT ) && ( ( pinfo - 1 )->int32dist > MID_DIST_LIMIT ) ) //���� ���� ���� Ż�� �ӵ� ���� �� | �� ���� �̸� ���� ���� �ʴ´�!!!!
				break;
		
			xsecession_for_angle_func( ( pinfo + 1 ) , perr , ( mark_cnt + 1 ) );
		
			if( ( ( pinfo - 1 )->int32down_flag == OFF ) && ( ( pinfo - 1 )->int32s44s_flag == OFF ) ) //������ ( ���� | ������ ) �̸� ������ ���� �ʴ´�.
			{
				m_dist = ( pinfo + 1 )->int32dist > MID_DIST_LIMIT ? ( float32 )( ( pinfo + 1 )->int32dist >> 2 ) : ( float32 )( ( pinfo + 1 )->int32dist >> 1 );
				xmemmove_sec_info_struct_func( pinfo , pinfo + 1 , ( float32 )( g_int32turn_vel + LIMIT_90_VEL ) , m_dist );	
			}
		}
		while( 0 );

		pinfo->fp32shift_after = pinfo->fp32shift_before;		

	}
	else
	{
		do
		{
			if( ( pinfo + 1 )->int32dir & ( TURN_45 | TURN_90 ) )  //���� ��
			{
				xsecession_for_angle_func( ( pinfo + 1 ) , perr , ( mark_cnt + 1 ) );				
				xcontinus_angle_vel_compute_func( pinfo , ( float32 )( pinfo->int32dist >> 1 ) , POS_KP_DOWN );		

				if( ( ( pinfo - 1 )->int32dir & STRAIGHT ) && ( ( pinfo - 1 )->int32dist > MID_DIST_LIMIT ) )  //���� ���� �߰� �̻�� ���� or �ٴ������� ū ���� ��� -> ����Ѵ�
				{
					pinfo->int32s44s_flag = ON;
					pinfo->int32down_flag = OFF;

					pinfo->fp32in_vel = ( float32 )( g_int32turn_vel );
					pinfo->fp32out_vel = pinfo->fp32vel = pinfo->fp32in_vel;
				}
				else if( !( ( pinfo + 1 )->int32dir & DIR_END ) && ( ( pinfo + 2 )->int32dir > TURN_180 ) )
				{
					pinfo->fp32kp_down = POS_KP_UP;
				
					pinfo->int32s44s_flag = OFF;
					pinfo->int32down_flag = OFF;

					pinfo->fp32in_vel = ( float32 )( g_int32turn_vel );
					pinfo->fp32out_vel = pinfo->fp32vel = pinfo->fp32in_vel;					

					ret = 1;
				}
				else;
				
			}
			else
			{
				if( mark_cnt > 2 && ( pinfo - 2 )->int32escape_flag == ON ) //���� ���� ���� Ż�� �ӵ� ���� �� �̸� ���� ���� �ʴ´�!!!!
					break;
				
				if( ( ( pinfo - 1 )->int32down_flag == OFF ) && ( ( pinfo - 1 )->int32s44s_flag == OFF ) && 
					( ( pinfo - 1 )->int32dir & STRAIGHT ) && ( ( pinfo - 1 )->int32dist < MID_DIST_LIMIT ) &&
					( ( pinfo + 1 )->int32dir & STRAIGHT ) && ( ( pinfo + 1 )->int32dist < MID_DIST_LIMIT ) )  //ª�� ���� - 90�� - ª�� ���� ������ �����Ѵ�!!
				{
					//���� �� -> ���� �� �ӵ����� 200 �� ���δ�.
					dist_to_maxvel_cpt( ( float32 )( pinfo->int32dist ) , ( float32 )( pinfo->int32dist >> 1 ) , ( float32 )( g_int32turn_vel ) ,  pinfo->int32acc , &pinfo->fp32vel );

					if( pinfo->fp32vel > ( float32 )( g_int32turn_vel + LIMIT_90_VEL ) )
						pinfo->fp32vel = ( float32 )( g_int32turn_vel + LIMIT_90_VEL );
					
					diffvel_to_remaindist_cpt( pinfo->fp32vel , ( float32 )( g_int32turn_vel ) , pinfo->int32acc , &pinfo->fp32decel_dist );			

					pinfo->fp32in_vel = pinfo->fp32out_vel = ( float32 )g_int32turn_vel;
				}
			}	
			
		}
		while( 0 );

		if( ret )
			pinfo->fp32shift_after = pinfo->fp32shift_before;
		else
			pinfo->fp32shift_after = ( ( pinfo + 1 )->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];			
		
	}

	pinfo->fp32dist_limit = pinfo->int32dist >> 1;	

	perr->fp32err_dist[ mark_cnt ] = ( float32 )( pinfo->int32dist >> 1 );
	perr->fp32err_dist[ mark_cnt ] += ( float32 )( pinfo->int32dist );  //������� �������� �����ش�.

	perr->fp32under_dist[ mark_cnt ] = ( float32 )( pinfo->int32dist >> 1 );	 //��ũ üũ ���� �Ÿ� ����.
	
}

static void x180angle_turn_compute( sec_info_t *pinfo , err_dps_t *perr , int32 mark_cnt )
{
	int32 shift = g_int32shift_level;

	pinfo->int32acc = g_int32turn_acc;

	pinfo->fp32kp_down = POS_KP_UP;	

	pinfo->fp32in_vel = ( float32 )( g_int32turn_vel );
	pinfo->fp32out_vel = pinfo->fp32vel = pinfo->fp32in_vel;

	pinfo->fp32shift_before = ( pinfo->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];
	
	if( ( pinfo + 1 )->int32dir & STRAIGHT )
		pinfo->fp32shift_after = ( pinfo->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];		
	else
		pinfo->fp32shift_after = ( ( pinfo + 1 )->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];

	pinfo->fp32dist_limit = ( float32 )( pinfo->int32dist ) * 0.65;

	perr->fp32err_dist[ mark_cnt ] = ( float32 )( pinfo->int32dist >> 1 );
	perr->fp32err_dist[ mark_cnt ] += ( float32 )( pinfo->int32dist );  //������� �������� �����ش�.

	perr->fp32under_dist[ mark_cnt ] = ( float32 )( pinfo->int32dist ) * 0.65;	 //��ũ üũ ���� �Ÿ� ����.

}


static void xdefault_turn_compute( sec_info_t *pinfo , err_dps_t *perr , int32 mark_cnt ) //default turn -> 270�� ����� �� ����
{
	int32 shift = g_int32shift_level;

	pinfo->int32acc = g_int32turn_acc;

	pinfo->fp32kp_down = POS_KP_UP;	

	pinfo->fp32in_vel = ( float32 )( g_int32turn_vel );
	if( g_int32turn_vel > LIMIT_SHIFT_VEL )
		pinfo->fp32in_vel = ( float32 )( LIMIT_SHIFT_VEL );
	
	pinfo->fp32out_vel = pinfo->fp32vel = pinfo->fp32in_vel;

	pinfo->fp32shift_before = ( pinfo->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];
	
	if( ( pinfo + 1 )->int32dir & STRAIGHT )	
		pinfo->fp32shift_after = ( pinfo->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];
	else
		pinfo->fp32shift_after = ( ( pinfo + 1 )->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];

	pinfo->fp32dist_limit = ( float32 )( pinfo->int32dist ) * 0.8;

	perr->fp32err_dist[ mark_cnt ] = ( float32 )( pinfo->int32dist >> 1 );
	perr->fp32err_dist[ mark_cnt ] += ( float32 )( pinfo->int32dist );  //������� �������� �����ش�.

	perr->fp32under_dist[ mark_cnt ] = ( float32 )( pinfo->int32dist ) * 0.65;	 //��ũ üũ ���� �Ÿ� ����.
	
}

static void xlarge_turn_compute( sec_info_t *p_info , err_dps_t *p_err , int32 mark_cnt )
{
	int32 shift = g_int32shift_level;

	sec_info_t *pinfo = p_info;
	err_dps_t *perr = p_err;

	volatile float32 big_vel = 0.0;
	volatile float32 small_vel = 0.0;

	pinfo->fp32kp_down = POS_KP_UP;
	pinfo->fp32in_vel = pinfo->fp32vel = pinfo->fp32out_vel = ( float32 )g_int32turn_vel;

	if( ( pinfo + 1 )->int32dir & STRAIGHT )  //������ �����̸� ���� �ְ� �ӵ��� Ż�� �Ѵ�.
	{
		xsecession_for_angle_func( ( pinfo + 1 ) , perr , ( mark_cnt + 1 ) );
		xmemmove_sec_info_struct_func( pinfo , pinfo + 1 , ( float32 )( g_int32large_vel ) , ( float32 )0.0 );
		pinfo->fp32in_vel = ( float32 )( g_int32turn_vel );
	}

	pinfo->int32down_flag = OFF;
	pinfo->int32s44s_flag = OFF;

	pinfo->fp32shift_before = ( pinfo->int32dir & RIGHT_TURN ) ? right_table[ shift + 2 ] : left_table[ shift + 2 ];
	pinfo->int32acc = g_int32large_acc;
	pinfo->fp32dist_limit = ( float32 )( pinfo->int32dist ) * 0.8;  //�� �Ÿ��� 65% ������ ����Ʈ ����

	if( ( pinfo + 1 )->int32dir & STRAIGHT )
		pinfo->fp32shift_after = pinfo->fp32shift_before;
	else
		pinfo->fp32shift_after = ( ( pinfo + 1 )->int32dir & RIGHT_TURN ) ? right_table[ shift ] : left_table[ shift ];
	
	big_vel = MAX( pinfo->fp32in_vel , pinfo->fp32out_vel );
	small_vel = MIN( pinfo->fp32in_vel , pinfo->fp32out_vel );
	diffvel_to_remaindist_cpt( pinfo->fp32in_vel , pinfo->fp32out_vel , pinfo->int32acc , &pinfo->fp32m_dist );  //�ӵ� �ٸ� ���� �Ÿ��� ���

	if( pinfo->fp32m_dist >= ( float32 )( pinfo->int32dist ) )  //������ ������ ���� �Ÿ����� Ŭ ��� -> ���� �ʿ�!!!
	{
		pinfo->fp32decel_dist = ( float32 )( pinfo->int32dist );
		dist_to_maxvel_cpt( ( float32 )( pinfo->int32dist ) , ( float32 )0.0 , small_vel ,  pinfo->int32acc , &( pinfo->fp32vel ) ); //���� �Ÿ��� ��ӵ����� �ְ� �ӵ��� ���

		if( pinfo->fp32in_vel > pinfo->fp32out_vel )	pinfo->fp32in_vel = pinfo->fp32vel;
		else											pinfo->fp32out_vel = pinfo->fp32vel;
	}
	else
	{
		dist_to_maxvel_cpt( ( float32 )( pinfo->int32dist ) , pinfo->fp32m_dist , big_vel ,  pinfo->int32acc , &( pinfo->fp32vel ) ); //���� �Ÿ��� ��ӵ����� �ְ� �ӵ��� ���
		diffvel_to_remaindist_cpt( pinfo->fp32vel , pinfo->fp32out_vel , pinfo->int32acc , &( pinfo->fp32decel_dist ) );   //���� �Ÿ� ���
	}

	if( pinfo->fp32vel > ( float32 )( g_int32large_vel ) )
		pinfo->fp32vel = ( float32 )( g_int32large_vel );
	
	if( ( pinfo - 1 )->int32dir & STRAIGHT )  //������ �����̸� ���� �ְ� �ӵ��� �����Ѵ�.
		pinfo->fp32in_vel = pinfo->fp32vel;

	perr->fp32err_dist[ mark_cnt ] = ( float32 )( pinfo->int32dist << 2 );  //�Ÿ� ���� üũ ��

	if( perr->fp32err_dist[ mark_cnt ] > ( float32 )( MID_DIST_LIMIT + SHT_DIST_LIMIT ) )  //���� �Ÿ��� �� ��� -> �����Ÿ�  �� ����.	
		perr->fp32err_dist[ mark_cnt ] = ( float32 )( MID_DIST_LIMIT + SHT_DIST_LIMIT ); 

	perr->fp32err_dist[ mark_cnt ] += ( float32 )( pinfo->int32dist );	//������� �������� �����ش�.

	perr->fp32under_dist[ mark_cnt ] = ( float32 )( pinfo->int32dist ) * 0.65;	//��ũ üũ ���� �Ÿ� ����.

}

static void xcontinus_angle_vel_compute_func( sec_info_t *p_info , volatile float32 dist , volatile float32 kp )
{
	sec_info_t *pinfo = p_info;

	pinfo->fp32kp_down = kp;
	pinfo->int32acc = g_int3245A_acc;

	pinfo->int32down_flag = ON;

	 //���ƿ��鼭 ��� �Ǿ���� �ϴ� ����.
	pinfo->fp32out_vel = ( pinfo + 1 )->fp32in_vel;
	dist_to_maxvel_cpt( ( float32 )( pinfo->int32dist ) , dist , pinfo->fp32out_vel ,  pinfo->int32acc , &( pinfo->fp32vel ) ); //���� �Ÿ��� ��ӵ����� �ְ� �ӵ��� ���
	diffvel_to_remaindist_cpt( pinfo->fp32out_vel , pinfo->fp32vel , pinfo->int32acc , &pinfo->fp32decel_dist );

	if( pinfo->fp32vel > ( float32 )( g_int3245A_vel ) )	pinfo->fp32vel = ( float32 )( g_int3245A_vel );  //�ְ� �ӵ� ����.

	pinfo->fp32in_vel = pinfo->fp32vel;	
	
}

static void xmemmove_sec_info_struct_func( sec_info_t *p_cur , sec_info_t *p_next , volatile float32 limit_vel , volatile float32 m_dist )
{
	sec_info_t cpy_info;
	sec_info_t *pinfo = p_cur;

	memset( ( void * )&cpy_info , 0x00 , sizeof( sec_info_t ) );

	memmove( ( void * )&cpy_info , ( const void * )p_next , sizeof( sec_info_t ) );	 //���� �� ������ info ������ copy -> ���� ����ϸ� call by reference �̹Ƿ� �ȵ�...
	dist_to_maxvel_cpt( ( float32 )( cpy_info.int32dist ) , m_dist , MIN( cpy_info.fp32in_vel , cpy_info.fp32out_vel ) , cpy_info.int32acc , &cpy_info.fp32vel );  //���� �� ������ ������ ���� �Ͽ����� �ְ�ӵ��� ����Ѵ�.
	
	if( cpy_info.fp32vel > limit_vel ) 								pinfo->fp32in_vel = limit_vel;
	else if( cpy_info.fp32vel < ( float32 )( g_int32turn_vel ) )	pinfo->fp32in_vel = ( float32 )( g_int32turn_vel );
	else															pinfo->fp32in_vel = cpy_info.fp32vel;
	
	pinfo->fp32vel = pinfo->fp32out_vel = pinfo->fp32in_vel;
}

static void xsecession_for_angle_func( sec_info_t *pinfo , err_dps_t *perr , int32 mark_cnt ) //����?? / 45��?? / 90��?? / 180��?? / 270��?? / ū��??
{
		 if( pinfo->int32dir & STRAIGHT 	)	xstraight_compute( pinfo , perr , mark_cnt );
	else if( pinfo->int32dir & TURN_45 		)	x45angle_turn_compute( pinfo , perr , mark_cnt );
	else if( pinfo->int32dir & TURN_90 		)	x90angle_turn_compute( pinfo , perr , mark_cnt );
	else if( pinfo->int32dir & TURN_180 	)	x180angle_turn_compute( pinfo , perr , mark_cnt );
	else if( pinfo->int32dir & TURN_270 	)	xdefault_turn_compute( pinfo , perr , mark_cnt );
	else if( pinfo->int32dir & LARGE_TURN 	)	xlarge_turn_compute( pinfo , perr , mark_cnt );
	else										xdefault_turn_compute( pinfo , perr , mark_cnt );
}

static void xmaxvel_compute_inadvance( void )
{
	int32 i;
	sec_info_t *pinfo = NULL;

	SCIa_Printf("\n\n");

	for( i = 0 ; i < g_int32total_mark ; i++ )
	{
		xsecession_for_angle_func( &g_secinfo[ i ] , &g_err , i );
	}

	if( g_int32total_mark < BUFF_MAX_SIZE )
	{
		pinfo = &g_secinfo[ g_int32total_mark ];
		memset( ( void * )pinfo , 0x00 , sizeof( sec_info_t ) );  //�Ѿ ���ǿ� ���� �ʱ�ȭ -> ����ó���� �ӵ����� 500 �������� 1�������Ѵ�.

		pinfo->int32acc = g_int32turn_acc;
		pinfo->fp32kp_down = POS_KP_UP;			
		pinfo->fp32in_vel = pinfo->fp32vel = pinfo->fp32out_vel = ( float32 )( g_int32turn_vel - 500 );
	}

#if 0
	SCIa_Printf("!! large mode %s !!\n\n" , g_int32large_turn_flag == ON ? "ON" : "OFF");

#if 1
	for( i = 0 ; i < g_int32total_mark ; i++ )	
	{
		SCIa_Printf("MARK[%3ld] : mkdir : %s dir : %s in_vel : %12lf vel : %12lf out_vel : %12lf acc : %2ld dist : %4ld dec_dist : %12lf err_dist : %12lf angle : %6ld l_dist : %4ld r_dist : %4ld abs : %4ld\n" , 
																												i , 
																												g_secinfo[ i ].pchmk_dir ,
																												g_secinfo[ i ].pchdir , 
																												g_secinfo[ i ].fp32in_vel  , 
																				   								g_secinfo[ i ].fp32vel , 
																				   								g_secinfo[ i ].fp32out_vel ,  
																				   								g_secinfo[ i ].int32acc , 
																				   								g_secinfo[ i ].int32dist , 
																				   								g_secinfo[ i ].fp32decel_dist , 
																				   								g_err.fp32err_dist[ i ] , 
																				   								g_secinfo[ i ].int32angle , 
																				   								g_secinfo[ i ].int32l_dist , 
																				   								g_secinfo[ i ].int32r_dist , 
																				   								g_secinfo[ i ].int32abs);
	}
#endif

	while( 1 )
	{
		DELAY_US( 1 );
	}
#endif

}

void xkval_ctrl_func( Uint32 mode , position_t *p_pos , float32 ratio , volatile float32 limit )
{
	position_t *ppos = p_pos;
	
	volatile float32 kval = 0.0;
	volatile float32 *pval = NULL;

	volatile float32 up_limit = 0.0;

	if( mode & KVAL_KP )
	{
		kval = ppos->fp32kp;
		pval = &ppos->fp32kp;

		up_limit = POS_KP_UP;
	}
	else		
	{
		kval = ppos->fp32kd;
		pval = &ppos->fp32kd;

		up_limit = POS_KD_UP;
	}

	if( mode & KVAL_UP )
	{
		kval += ( ratio * g_fp32shift_dist );
		if( kval > up_limit )
			kval = up_limit;
	}
	else
	{
		kval -= ( ratio * g_fp32shift_dist );
		if( kval < limit )
			kval = limit;	
	}

	*pval = kval;
	
}


static void xpos_shift_func( volatile float32 cur_dist , volatile float32 shift_dist , sec_info_t *p_info )
{
	sec_info_t *pinfo = p_info;

	volatile float32 c_dist = cur_dist;
	volatile float32 s_dist = shift_dist;

	volatile float32 pre_ratio = ( pinfo->int32dir & STRAIGHT ) && ( pinfo->int32dist > MID_DIST_LIMIT ) ? ST_RET_RATIO : SHIFT_RATIO;
	volatile float32 aft_ratio = ( pinfo->int32dir & STRAIGHT ) && ( pinfo->int32dist > MID_DIST_LIMIT ) ? ST_RET_RATIO : RETURN_RATIO;		
	
	volatile float32 pos_val = g_fp32shift_pos_val;


	if( g_Flag.cross_shift )
		return;

	if( g_Flag.err )
	{
		if( pos_val > ( float32 )( 0.0 ) )			pos_val -= ( g_fp32shift_dist * SHIFT_RATIO );
		else if( pos_val < ( float32 )( 0.0 ) )		pos_val += ( g_fp32shift_dist * SHIFT_RATIO );
		else										pos_val = ( float32 )( 0.0 );		

		g_fp32shift_pos_val = pos_val;
		
		return;
	}

	if( c_dist < pinfo->fp32dist_limit )
	{
		if( pos_val > pinfo->fp32shift_before + PM_RATIO )			pos_val -= ( s_dist * pre_ratio );
		else if( pos_val < pinfo->fp32shift_before - PM_RATIO )		pos_val += ( s_dist * pre_ratio );
		else														pos_val = pinfo->fp32shift_before;
	}
	else
	{
		if( pos_val > pinfo->fp32shift_after + PM_RATIO )			pos_val -= ( s_dist * aft_ratio );
		else if( pos_val < pinfo->fp32shift_after - PM_RATIO )		pos_val += ( s_dist * aft_ratio );
		else
		{
			if( pinfo->int32dir > TURN_180 )
				YELLOW_ON;
			
			pos_val = pinfo->fp32shift_after;	
		}
	}		

	g_fp32shift_pos_val = pos_val;	
	
}


void extreme_run( sec_info_t *p_info )
{
	sec_info_t *pinfo = p_info;
	volatile float32 turn_vel = ( float32 )( g_int32turn_vel + LIMIT_90_VEL );  //���� �ϼӵ� ���� �ణ ����!!
	
	volatile float32 shift_dist;

	if( turn_vel > ( float32 )( LIMIT_SHIFT_VEL ) ) //2800�� �ѱ��� �ʴ´�
		turn_vel = ( float32 )( LIMIT_SHIFT_VEL );

	shift_dist = turn_vel * SAMPLE_FRQ;  //���ͷ�Ʈ �ֱ�� �Ÿ�

	g_Flag.xrun = ON;  // 3�� ����.
	g_Flag.goal_dest = ON; // 2�� ����

	if( gyro_center_value_search( &g_gyro ) < 0 )
		return;
	
	VFDPrintf("Run_Time");
	DELAY_US( 240000 );	
	VFDPrintf("        ");

	race_start_init(); //���� �ϱ� �� �ݵ�� �ʱ�ȭ �ٽ� �Ǿ�� �ϴ� ������ ����

	line_load_rom();  //����� ���� �ε�

	if( g_int32inverse_run == ON )
		inverse_run_info_compute();	

	xmaxvel_compute_inadvance(); //���� �� �̸� ����� �ӵ� �� ���ӵ� ���

	if( pinfo->int32dir & STRAIGHT ) //ù ����� ������ ���.	
	{
		BLUE_ON;
		g_Flag.speed_up_start = ON; //���� ���� �÷��� ON
	}
	else
		g_Flag.straight = OFF;

	handle_ad_make( OUT_CONER_LIMIT , FASTRUN_IN_CONER_LIMIT ); //�ڵ�� ���	
	move_to_move( ( float32 )( pinfo->int32dist ) , pinfo->fp32decel_dist , pinfo->fp32vel , pinfo->fp32out_vel , pinfo->int32acc );

	g_Flag.start_flag = ON;	

	while( 1 )
	{
		g_fp32xrun_dist = ( L_motor.fp32gone_distance + R_motor.fp32gone_distance ) * 0.5;
		xpos_shift_func( g_fp32xrun_dist , shift_dist , &g_secinfo[ g_int32mark_cnt ] );
	
		position_compute(); //������ ����

		if( g_Flag.move_state ) //���� ���϶���...
		{
			g_lmark.fp32turn_dis = ( g_lmark.fp32check_dis + g_rmark.fp32check_dis ) * 0.5; //�ϸ�ũ üũ �Ÿ��� ����
			g_rmark.fp32turn_dis = g_lmark.fp32turn_dis;

			mark_checking_func( g_ptr->plmark , g_ptr->prmark ); //���� �ϸ�ũ üŷ
			mark_checking_func( g_ptr->prmark , g_ptr->plmark ); //������ �ϸ�ũ üŷ
		}

		if( g_int32pid_ISR_cnt ) //���� interrupt ����ȭ
		{
			if ( line_out_func() ||
			     race_stop_check() )
			     return;

			speed_up_compute( pinfo ); //���� ���� �÷��� ��ٸ��� �Լ�
			second_error_disposal( &g_err ,  pinfo , g_int32mark_cnt );  //��ũ ����ó��

			g_int32pid_ISR_cnt = 0; //���� ����
		}		
		
	}
	
}


