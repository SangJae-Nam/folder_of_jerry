//###########################################################################
//
// FILE		: search.c
//
// TITLE		: micromouse H2 search source file.
//
// Author	: leejaeseong
//
// Company	: Hertz
//
//###########################################################################
// $Release Date: 2012.03.24 $
//###########################################################################

#include "DSP2833x_Device.h"     // DSP2833x Headerfile Include File
#include "DSP2833x_Examples.h"   // DSP2833x Examples Include File

static int32 _search_type_select_func( void )
{
	const char *type[] = { "clr " , "rsch" , "rsh1" , "rsh2" , "rsh3" };

	sw_t sw;
	memset( ( void * )&sw , 0x00 , sizeof( sw_t ) );

	vfdprintf( ( char * )type[ 0 ] );

	while( TRUE )
	{		
		if( !UP_SW && sw.u16up == OFF ) 
		{
			GREEN_ON;
		
			sw.u16up = ON;
			g_int32menu_cnt = ZERO;
		}
		else if( !RIGHT_SW && sw.u16right == OFF )
		{
			YELLOW_ON;
		
			if( sw.int32col )
				maze_map_read_rom( sw.int32col - 1 );
			
			break;
		}
		else if( UP_SW && RIGHT_SW )
		{
			if( sw.u16up == ON && g_int32menu_cnt > KEY_CHATTERING )
			{
				SWU_BELL;
				sw.u16up = OFF;
		
				sw.int32col++;
				if( sw.int32col >= 5 )	sw.int32col = ZERO;
			}
			else
			{
				LED_OFF;
				sw.u16up = OFF;
			}					
		}
		else;
		
		if( sw.int32vfd_chatt_col != sw.int32col )
		{
			vfdprintf( ( char * )type[ sw.int32col ] );
			sw.int32vfd_chatt_col = sw.int32col;			
		}
		
	}	

	LED_OFF;
	h2_delay();

	return 0;

}

static int32 _research_type_select_func( int32 *turn_vel , int32 *vel_sel )
{
	const char *type[] = { " 800" , "1000" , "1200" };
	const mouse_speed_e vel[ 3 ] = { S800 , S1000 , S1200 };
	const turn_vel_e sel[ 3 ] = { V_800 , V_1000 , V_1200 };

	sw_t sw;
	memset( ( void * )&sw , 0x00 , sizeof( sw_t ) );

	vfdprintf( ( char * )type[ 0 ] );

	while( TRUE )
	{		
		if( !UP_SW && sw.u16up == OFF ) 
		{
			GREEN_ON;
		
			sw.u16up = ON;
			g_int32menu_cnt = ZERO;
		}
		else if( !RIGHT_SW && sw.u16right == OFF )
		{
			YELLOW_ON;
			
			*turn_vel = ( int32 )vel[ sw.int32col ];
			*vel_sel = ( int32 )sel[ sw.int32col ];
			
			break;
		}
		else if( UP_SW && RIGHT_SW )
		{
			if( sw.u16up == ON && g_int32menu_cnt > KEY_CHATTERING )
			{
				SWU_BELL;
				sw.u16up = OFF;
		
				sw.int32col++;
				if( sw.int32col >= 3 )	sw.int32col = ZERO;
			}
			else
			{
				LED_OFF;
				sw.u16up = OFF;
			}					
		}
		else;
		
		if( sw.int32vfd_chatt_col != sw.int32col )
		{
			vfdprintf( ( char * )type[ sw.int32col ] );
			sw.int32vfd_chatt_col = sw.int32col;			
		}
		
	}	

	LED_OFF;
	h2_delay();

	return 0;

}

int32 race_start_init( int32 *turn_vel , int32 *vel_sel )
{
	LED_OFF;
	
	POS_CPN_ON;
	ANGLE_CPN_ON;		 //���� ON!!
	motor_vari_init();	 //���� ���� �ʱ�ȭ.

	edge_vari_init();	 //���� ���� �ʱ�ȭ.

	if( g_flag.fastrun == OFF ) 
	{
		_search_type_select_func(); 						//Ž�� ���� ����!!
		_research_type_select_func( turn_vel , vel_sel );	//���ƿ� �� 2�� �ӵ� ����.
	}
	else
		run_acc_select_func();								//2�� ����� ���ӵ� ����.

	g_flag.start = ON;
	move_end( ( float32 )0.0 , ( float32 )0.0 , ( float32 )0.0 , ( float32 )0.0 , 1 );	 //0�� ���� ����.		

	if( init_gyro_reference_value_search( &g_gyro ) < 0 )	 //���̷� ���Ͱ�.
		goto error;

	while( RIGHT_SW );
	h2_delay();

	vfdprintf("    ");

	DELAY_US( 400000 );

	return 0;

error :
	return -1;
	
}

void search_run_mode( void )
{
	Uint16 path_cnt;
	Uint16 state = ( Uint16 )STRAIGHT;

	int32 turn_vel = SEARCH_S;
	int32 vel_sel = V_650;

	algorithm_vari_init();

	if( race_start_init( &turn_vel , &vel_sel ) < 0 )
		return;

	g_flag.fastrun = OFF;

	g_run.fp32str_limit_vel = BASE_VEL_LIMIT;
	g_run.fp32diag_limit_vel = BASE_VEL_LIMIT;

	g_run.fp32str_limit_acc = BASE_ACC_LIMIT;
	g_run.fp32diag_limit_acc = BASE_ACC_LIMIT;

	algorithm( g_u16map[ g_u16mouse_pos ] ); //ù �� �˰��� ������.

	known_path_printf();

	move_to_move( ONE_BLOCK , ( float32 )g_int32turn_vel , ( float32 )g_int32turn_vel , g_int32turn_acc );  //���!!

	g_rmotor.fp32distance_sum = g_lmotor.fp32distance_sum = ( float32 )70.0; //ù�� �Ÿ� ����.

	g_u32time_cnt = 0;
	g_flag.algo = ON; //�˰��� ON!

	while( ( state != BACKTURN ) || ( g_flag.search_end != ON ) || ( g_u16yet_mpos != 0x00 ) ) //���������� ���ƿ� �� ����.
	{
		path_cnt = g_u16path_cnt;
		g_u16path_cnt = 0;

		g_flag.algo = ( path_cnt > 0 ) ? OFF : ON;

		while( 1 )
		{	
			state = g_knownpath[ g_u16path_cnt ].u8state; //�ƴ±� �� ���� �޾� ��.

			if( g_u16path_cnt == ( path_cnt - 1 ) && g_flag.algo == OFF ) //�ƴ±� -> �𸣴� �� ����.
				g_flag.algo = ON;

			if( *pselect_func[ state ] == NULL )
			{
				LED_ON;
			
				vfdprintf("%u" , state);
				MOTOR_OFF;
			
				while( 1 );
			}
			
			pselect_func[ state ](); //�� ���ǿ� �´� �� ����.

			if( g_flag.algo == ON )
				break;

			g_u16path_cnt++; //�ε��� ����.

		}
		
	}

	while( 1 )
	{
		//2�� �ӵ� ����.
		g_int32turn_vel = turn_vel;
		g_int32vel_select = vel_sel;

		recursive_fastrun_code_func();	
	}
	
}

