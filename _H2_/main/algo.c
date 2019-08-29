//###########################################################################
//
// FILE		: algo.c
//
// TITLE		: micromouse H2 algorithm source file.
//
// Author	: leejaeseong
//
// Company	: Hertz
//
//###########################################################################
// $Release Date: 2012.03.19 $
//###########################################################################

#include "DSP2833x_Device.h"     // DSP2833x Headerfile Include File
#include "DSP2833x_Examples.h"   // DSP2833x Examples Include File

#define GARBAGE_DATA		( Uint16 )0xffff

typedef enum weight_write_state
{
	START_BLOCK , 
	ETC_BLOCK
	
}weight_write_state_e;

typedef volatile struct linked_list
{
	Uint16 u16pos;
	Uint16 u16weight;

	volatile struct linked_list *plink;
	
}list_t;

typedef volatile struct queue
{
	list_t *phead;
	list_t *prear;
	
}queue_t;

typedef volatile enum mouse_run_state
{
	STRAIGHT_STATE , 
	DIAGONAL_STATE
	
}mouse_run_state_e;

queue_t *g_que = NULL;
mouse_run_state_e a_emhead;
turn_class_e a_eturn_state;

const Uint16 a_u16strai_weight_t[ 32 ] = { 17 , 12 , 10 , 9 , 8 , 7 , 7 , 7 , 
											7 , 7 , 7 , 7 , 7 , 7 , 7 , 7 , 
											7 , 7 , 7 , 7 , 7 , 7 , 7 , 7 , 
											7 , 7 , 7 , 7 , 7 , 7 , 7 , 7 };

const Uint16 a_u16diag_weight_t[ 32 ] = { 22 , 15 , 12 , 10 , 10 , 10 , 10 , 10 , 
										  10 , 10 , 10 , 10 , 10 , 10 , 10 , 10 , 
										  10 , 10 , 10 , 10 , 10 , 10 , 10 , 10 , 
										  10 , 10 , 10 , 10 , 10 , 10 , 10 , 10 };

static int32 _memory_allocation( void **ptr , size_t size );
static void _memory_free( void **ptr );
static int32 _insert_data_in_queue( queue_t *pque , Uint16 pos , Uint16 weight );
static int32 _desert_data_by_queue( queue_t *pque );
static void _delete_all_data_in_queue( queue_t *pque );

static void _maze_weight_write_func( weight_write_state_e mode , Uint16 now_pos );
static void _known_block_path_make_func( Uint16 pos , Uint16 dir );
static void all_case_path_make_func( mouse_run_type_e type );

#if 0
#pragma CODE_SECTION(_maze_weight_write_func, "ramfuncs");
#pragma CODE_SECTION(_insert_data_in_queue, "ramfuncs");
#pragma CODE_SECTION(_desert_data_by_queue, "ramfuncs");
#pragma CODE_SECTION(_delete_all_data_in_queue, "ramfuncs");
#pragma CODE_SECTION(_known_block_path_make_func, "ramfuncs");
#endif

static void _init_cordinate_in_queue( queue_t *pque )
{
	int i;

	memset( ( void * )g_minfo , 0x00 , sizeof( g_minfo ) );

	for( i = 0 ; i < MAX_MAZE ; i++ )
		g_minfo[ i ].all = 0x0000ffff;

	if( g_flag.goal == GO_GOAL ) //���!!
	{
		g_minfo[ 0x77 ].bit.u16weight = 0x00;
		g_minfo[ 0x78 ].bit.u16weight = 0x00;
		g_minfo[ 0x88 ].bit.u16weight = 0x00;
		g_minfo[ 0x87 ].bit.u16weight = 0x00;

		g_minfo[ 0x77 ].bit.now_vector = DIR_W;
		g_minfo[ 0x78 ].bit.now_vector = DIR_N;
		g_minfo[ 0x88 ].bit.now_vector = DIR_E;
		g_minfo[ 0x87 ].bit.now_vector = DIR_S;		

		_insert_data_in_queue( pque , 0x77 , 0x00 );
		_insert_data_in_queue( pque , 0x78 , 0x00 );
		_insert_data_in_queue( pque , 0x88 , 0x00 );
		_insert_data_in_queue( pque , 0x87 , 0x00 );
	}
	else //���ƿ���!!
	{
		g_minfo[ 0x00 ].bit.u16weight = 0x00;
		g_minfo[ 0x00 ].bit.now_vector = DIR_N;

		_insert_data_in_queue( pque , 0x00 , 0x00 );
	}
	
}

static void _maze_info_init_func( void )
{
	Uint16 i;

	g_u16mouse_pos = 0x00;
	g_u16mouse_dir = 0x00;
	g_u16yet_mpos = 0x00;
	g_u16yet_mdir = 0x00;
	
	g_u16path_cnt = 0;
	g_int32block_to_block = TURN_TO_STRAIGHT;
	
	g_flag.goal = GO_GOAL;
	g_flag.search_end = OFF;
	g_flag.second_rungoal = OFF;
	g_flag.first_block_diag = OFF;

	memset( ( void * )g_u16map , 0x00 , sizeof( g_u16map ) );
	memset( ( void * )g_u16map_backup0 , 0x00 , sizeof( g_u16map_backup0 ) );
	memset( ( void * )g_u16map_backup1 , 0x00 , sizeof( g_u16map_backup1 ) );
	memset( ( void * )g_u16map_backup2 , 0x00 , sizeof( g_u16map_backup2 ) );	
	memset( ( void * )g_u16map_backup3 , 0x00 , sizeof( g_u16map_backup3 ) );
	
	memset( ( void * )g_knownpath , 0x00 , sizeof( g_knownpath ) );

	_delete_all_data_in_queue( g_que );

	for( i = 0 ; i < MAX_MAZE ; i++ ) //�ܰ� �� ����.
	{
		if( ( i & 0xf0 ) == 0xf0 )		g_u16map[ i ] |= DIR_E;
		else if( ( i & 0xf0 ) == 0x00 )	g_u16map[ i ] |= DIR_W;
		else;

		if( ( i & 0x0f ) == 0x0f )		g_u16map[ i ] |= DIR_N;
		else if( ( i & 0x0f ) == 0x00 )	g_u16map[ i ] |= DIR_S;
		else;		
	}

	g_u16map[ 0x00 ] |= 0x0e; //������ ����.
	
}

static void _start_algo_vari_init( void )
{
	_memory_allocation( ( void ** )&g_que , sizeof( queue_t ) );

	_init_cordinate_in_queue( g_que );  //���԰� �ʱ�ȭ.

	_maze_info_init_func(); 			//�� ���� �ʱ�ȭ.

	_maze_weight_write_func( ETC_BLOCK , 0x00 ); //�ʱ�ȭ �� �������� ������ ���԰� �Ѹ���...
}

static void _end_algo_vari_init( void ) //���α׷��� ������ ȣ���Ѵ� -> ���� �� ����!!
{
	_delete_all_data_in_queue( g_que );
	_memory_free( ( void ** )&g_que );	
}

void algorithm_vari_init( void )
{
	_start_algo_vari_init();
}

static void _maze_weight_write_func( weight_write_state_e mode , Uint16 now_pos ) //���԰� �Ѹ��� �Լ�!!
{
	int32 i = 0;
	int32 cordi = 0 , next_cordi = 0;

	int16 head_state = 0;	

	Uint16 wall = 0;
	Uint16 sum_weight = 0 , next_weight = 0;

	while( ( cordi = _desert_data_by_queue( g_que ) ) != -1 ) //queue�� ��� �� ������ �ݺ�.
	{
		if( cordi < 0 || cordi > 0xff )
		{
			MOTOR_OFF;
		
			vfdprintf("err0");
			while( 1 );
		}

		if( ( mode != START_BLOCK ) && ( g_minfo[ now_pos ].bit.u16weight < g_minfo[ cordi ].bit.u16weight ) ) //���� ��ġ���� ���԰��� �ѷ����� Ż���Ѵ�.
		{
			_delete_all_data_in_queue( g_que );
			return;
		}

		wall = g_u16map[ cordi ] & 0x0f;

		for( i = 0 ; i < 4 ; i++ ) //��� ����.
		{
			if( !( wall & g_u16wall_t[ i ] ) ) //���� ���� ���� �˻��Ѵ�.
			{
				if( g_u16wall_t[ i ] & g_minfo[ cordi ].bit.now_vector ) //����
				{
					head_state = F_STRAIGHT;
					sum_weight = a_u16strai_weight_t[ g_minfo[ cordi ].bit.block_cnt ]; //sum_weight�� ���Ⱚ�� ���� �� �� �ִ�!!
				}
				else
				{
					if( g_u16wall_t[ i ] == g_minfo[ cordi ].bit.yet_vector ) //�밢
					{
						head_state = DIAGONAL;
						sum_weight = a_u16diag_weight_t[ g_minfo[ cordi ].bit.block_cnt ];
					}
					else //��
					{
						head_state = TURN;
						sum_weight = TURN_WEIGHT;
					}
				}
				
				if( g_minfo[ cordi + g_int32move_t[ i ] ].bit.u16weight > ( g_minfo[ cordi ].bit.u16weight + sum_weight ) ) //���� ��ġ ������ NESW ���԰��� ���� ���԰����� Ŭ ���.
				{
					next_cordi = ( cordi + g_int32move_t[ i ] );
					next_weight = g_minfo[ cordi ].bit.u16weight + sum_weight;

					if( next_cordi < 0 || next_cordi > 0xff ) //���� ó��.
					{
						MOTOR_OFF;
					
						vfdprintf("err1");
						while( 1 );
					}

					//�̷� ���� ������Ʈ.
					if( head_state == TURN )
					{
						g_minfo[ next_cordi ].bit.block_cnt = 0;
						g_minfo[ next_cordi ].bit.block_state = TURN;
					}
					else if( head_state == DIAGONAL )
					{
						g_minfo[ next_cordi ].bit.block_cnt = ( g_minfo[ cordi ].bit.block_state == DIAGONAL ) ? g_minfo[ cordi ].bit.block_cnt + 1 : 0;
						g_minfo[ next_cordi ].bit.block_state = DIAGONAL;
					}
					else if( head_state == F_STRAIGHT )
					{
						g_minfo[ next_cordi ].bit.block_cnt = ( g_minfo[ cordi ].bit.block_state == F_STRAIGHT ) ? g_minfo[ cordi ].bit.block_cnt + 1 : 0;
						g_minfo[ next_cordi ].bit.block_state = F_STRAIGHT;
					}
					else;

					g_minfo[ next_cordi ].bit.yet_vector = g_minfo[ cordi ].bit.now_vector;
					g_minfo[ next_cordi ].bit.u16weight = next_weight;
					g_minfo[ next_cordi ].bit.now_vector = g_u16wall_t[ i ];

					_insert_data_in_queue( g_que , next_cordi , next_weight ); //���԰� ������ �� �ش� ��ǥ�� queue�� �ִ´�.
					
				}			

			}
		
		}
	
	}
	
}


void algorithm( Uint16 wallinfo )
{
	int32 i;
	int32 min_weight , next_weight , known_flag = 0;

	Uint16 ex_cordi = 0x00;
	Uint16 pos = 0;	
	Uint16 next_dir = 0 , turn_dir = 0;

	int32 next_pos = 0;

	const Uint16 ex_wall_info_t[] = { DIR_S , DIR_W , DIR_N , DIR_E };
	const Uint16 err_map_t[] = { 0x0f , 0xf0 , 0x0f , 0xf0 , 0x0f , 0xf0 , 0x00 , 0x00 };

	pos = g_u16mouse_pos;

	if( !( g_u16map[ pos ] & 0x10 ) ) //�ƴ±� �ƴ� ��!!
	{
		g_u16map[ pos ] |= ( 1 << 4 );			 //�ƴ� ��.
		g_u16map[ pos ] |= ( wallinfo & 0x0f );	 //�� ���� ������Ʈ...

		for( i = 0 ; i < 4 ; i++ )
		{
			ex_cordi = pos + g_int32move_t[ i ];
			if( wallinfo & g_u16wall_t[ i ] )		 //���� ���� ���� �˻��Ѵ�.
			{
				if( !( ( pos & err_map_t[ i ] ) == err_map_t[ i + 4 ] ) )	//�ܰ� ���� �ƴ� ��쿡�� ������Ʈ!!
					g_u16map[ ex_cordi ] |= ex_wall_info_t[ i ];			//���� ������ ��� �����ǿ����� ���� �������� �����Ѵ�.
			}				
		}
	}
	else
		known_flag = 1;

	if( !known_flag || pos == 0x00 || pos == 0x77 || pos == 0x78 || pos == 0x87 || pos == 0x88 ) //�ƴ� ���� ���԰� ���� �ʿ� ����.
	{
		_init_cordinate_in_queue( g_que );
		_maze_weight_write_func( ETC_BLOCK , pos );
	}

	min_weight = g_minfo[ pos ].bit.u16weight;

	for( i = 0 ; i < 4 ; i++ )
	{
		if( !( ( g_u16map[ pos ] & 0x0f ) & g_u16wall_t[ i ] ) ) //���� ���� ���� �������� ���� ���.
		{
			next_weight = g_minfo[ pos + g_int32move_t[ i ] ].bit.u16weight;

			if( next_weight < min_weight ) //���԰��� ���� ������ �̵��Ѵ�!!
			{
				min_weight = next_weight;
				next_pos = ( int32 )pos + g_int32move_t[ i ];

				turn_dir = i;											 //���� ���� ���� ���.
				next_dir = ( turn_dir + 4 - g_u16mouse_dir ) & 0x03;	 //���� ��� ���� ���.
			}
		}
	}

	if( next_pos < 0 || next_pos > 0xff )
	{
		vfdprintf("err5");
		MOTOR_OFF;
		while( 1 );	
	}

	//���� ��� �� ����.
	g_u16yet_mdir = g_u16mouse_dir;
	g_u16mouse_dir = turn_dir;

	//������ ��� �� ����.
	g_u16yet_mpos = g_u16mouse_pos;
	g_u16mouse_pos = ( Uint16 )next_pos;

	//���� �� �ƴ±�� �������.
	g_u16path_cnt = 0;

	g_knownpath[ 0 ].u4t_dir = next_dir;
	g_knownpath[ 0 ].u8state = next_dir;
	g_knownpath[ 0 ].u8pos = g_u16yet_mpos;
	g_knownpath[ 0 ].u4m_dir = g_u16yet_mdir;
	g_knownpath[ 0 ].u8cnt = 1;
	g_knownpath[ 1 ].u8state = LAST_PATH;

	//goal? or start?
	if( min_weight == 0 )
	{
		if( g_flag.goal == GO_GOAL )		g_flag.goal = GO_START;
		else if( g_flag.goal == GO_START )	g_flag.search_end = ON;
		else;
	}

	if( ( g_u16map[ next_pos ] & 0x10 ) && ( min_weight != 0 ) && ( next_dir != BACKTURN ) ) //�ƴ±� �о� ����.
		_known_block_path_make_func( ( Uint16 )next_pos , turn_dir );
		
}

static void _known_block_path_make_func( Uint16 pos , Uint16 dir )
{
	int32 i;

	Uint16 next_dir;
	Uint16 turn_dir;
	Uint16 next_pos;

	Uint16 min_weight;
	Uint16 next_weight;

	g_u16path_cnt = 1;

	do
	{
		min_weight = g_minfo[ pos ].bit.u16weight;

		for( i = 0 ; i < 4 ; i++ )
		{
			if( !( ( g_u16map[ pos ] & 0x0f ) & g_u16wall_t[ i ] ) ) //���� ���� �������� ���´�.
			{
				next_weight = g_minfo[ pos + g_int32move_t[ i ] ].bit.u16weight;

				if( next_weight < min_weight ) //���԰� ���� ������ �̵�.
				{
					min_weight = next_weight;
					next_pos = pos + g_int32move_t[ i ];
					turn_dir = i;
					next_dir = ( turn_dir + 4 - dir ) & 0x03;
				}
			}
		}

		g_knownpath[ g_u16path_cnt ].u8pos = pos;
		pos = next_pos;

		g_knownpath[ g_u16path_cnt ].u4m_dir = dir;
		dir = turn_dir;

		g_knownpath[ g_u16path_cnt ].u4t_dir = next_dir;

		g_u16path_cnt++;
		
	}
	while( ( g_u16map[ pos ] & 0x10 ) && ( min_weight != 0 ) && ( next_dir != BACKTURN ) ); //�˰� �ִ� �н����� �ݺ��ؼ� ã�� ���� ������ ������ ����.

	//�ƴ� �� ������ ������ �� ���� ����� ���� -> �𸣴±� �����ǰ� ������ ����.
	g_u16mouse_pos = pos;
	g_u16mouse_dir = dir;

	if( min_weight == 0 )	
	{
		if( g_flag.goal == GO_GOAL )		g_flag.goal = GO_START;
		else if( g_flag.goal == GO_START )
		{
			g_flag.search_end = ON;
		
			g_knownpath[ g_u16path_cnt ].u8pos = pos;
			g_knownpath[ g_u16path_cnt ].u4m_dir = dir;
			g_knownpath[ g_u16path_cnt ].u4t_dir = BACKTURN;
			g_knownpath[ g_u16path_cnt ].u8state = BACKTURN;

			g_u16path_cnt++;
		}
		else;
	}	

	all_case_path_make_func( SEARCH );
	
}

void fastrun_path_make_func( void )
{
	Uint16 cnt , i;
	
	Uint16 ex_wall_info[] = { DIR_S , DIR_W , DIR_N , DIR_E };	
	Uint16 error_map[] = { 0x0f , 0xf0 , 0x0f , 0xf0 , 0x0f , 0xf0 , 0x00 , 0x00 };
	
	Uint16 goal[ 4 ] = { 0 , };
	Uint16 low_weight = 0xffff;
	Uint16 goal_pos = 0;

	Uint16 next_dir = 0;
	Uint16 turn_dir = 0;
	Uint16 mouse_dir = 0;

	Uint16 cur_pos = 0;
	Uint16 next_pos = 0;
	
	Uint16 min_weight;
	Uint16 next_weight;

	memset( ( void * )g_u16map , 0x00 , sizeof( g_u16map ) );
	memset( ( void * )g_u16map_backup0 , 0x00 , sizeof( g_u16map_backup0 ) );

	SpiLoadRom( ( Uint16 )( MAZE_MAP_BACKUP0_PAGE ) , 0x00 ,  ( Uint16 )( MAZE_MAX_BLOCK ) , ( Uint16 * )g_u16map_backup0 ); //����� �̷� �ҷ� ��.

	for( cnt = 0 ; cnt < MAX_MAZE ; cnt++ ) //����� �̷� ���� ���� map���� ������Ʈ
	{
		if( ( g_u16map_backup0[ cnt ] >> 4 ) & 0x01 )							 //�ƴ� ���� ���� ����.
			g_u16map[ cnt ] |= ( g_u16map_backup0[ cnt ] & 0x0f );
		else
		{
			g_u16map[ cnt ] = ( DIR_N | DIR_E | DIR_S | DIR_W );		 		//�𸣴� ���� F�� ���´�.
			for( i = 0 ; i < 4 ; i++ )
			{
				if( !( cnt & error_map[ i ] == error_map[ i + 4 ] ) )	 		//�ܰ� ���� �ƴ� ���.
					g_u16map[ cnt + g_int32move_t[ i ] ] |= ex_wall_info[ i ];	//���� ������ ��� �����ǿ����� ���� �������� �����Ѵ�.
			}
		}
	}

	g_flag.goal = GO_START;

	//���԰� �ٽ� �Ѹ���.
	_delete_all_data_in_queue( g_que );
	_init_cordinate_in_queue( g_que );
	_maze_weight_write_func( START_BLOCK , 0x00 );
	_delete_all_data_in_queue( g_que );

	//�� ���� ���� �� ������ ����.
	cnt = 0;
	if( !( g_u16map[ 0x77 ] & DIR_W ) || !( g_u16map[ 0x77 ] & DIR_S ) )
		goal[ cnt++ ] = 0x77;
	if( !( g_u16map[ 0x78 ] & DIR_W ) || !( g_u16map[ 0x78 ] & DIR_N ) )
		goal[ cnt++ ] = 0x78;
	if( !( g_u16map[ 0x87 ] & DIR_E ) || !( g_u16map[ 0x87 ] & DIR_S ) )
		goal[ cnt++ ] = 0x87;
	if( !( g_u16map[ 0x88 ] & DIR_E ) || !( g_u16map[ 0x88 ] & DIR_N ) )
		goal[ cnt++ ] = 0x88;

	for( cnt = 0 ; cnt < 4 ; cnt++ )
	{
		if( goal[ cnt ] && ( low_weight > g_minfo[ goal[ cnt ] ].bit.u16weight ) )
		{
			low_weight = g_minfo[ goal[ cnt ] ].bit.u16weight;
			goal_pos = goal[ cnt ];
		}
	}

	mouse_dir = 0;
	g_u16path_cnt = 0;

	cur_pos = goal_pos;
	min_weight = g_minfo[ goal_pos ].bit.u16weight;

	memset( ( void * )g_knownpath , 0x00 , sizeof( g_knownpath ) );

	while( 1 ) //goal���� ������ ���� u16pos �� �̾Ƴ���.
	{
		for( i = 0 ; i < 4 ; i++ )
		{
			if( !( ( g_u16map[ cur_pos ] & 0x0f ) & g_u16wall_t[ i ] ) ) //���� ���� ��.
			{
				next_weight = g_minfo[ cur_pos + g_int32move_t[ i ] ].bit.u16weight;
				if( next_weight < min_weight )
				{
					min_weight = next_weight;
					next_pos = cur_pos + g_int32move_t[ i ];
					turn_dir = i;
					next_dir = ( turn_dir + 4 - mouse_dir ) & 0x03;
				}
			}
		}

		g_knownpath[ g_u16path_cnt ].u8pos = cur_pos;

		cur_pos = next_pos;
		mouse_dir = turn_dir;
		g_u16path_cnt++;

		if( g_u16path_cnt > MAX_MAZE - 1 )
		{
			vfdprintf("err3");
			MOTOR_OFF;
			while( 1 );
		}

		if( next_pos == 0x00 )
		{
			g_knownpath[ g_u16path_cnt++ ].u8pos = 0x00;
			break;
		}
		
	}

	for( cnt = 0 ; cnt < MAX_MAZE ; cnt++ )		 //���԰� ��� ffff�� ���� ��.
		g_minfo[ cnt ].bit.u16weight = 0xffff;

	for( cnt = 0 ; cnt < g_u16path_cnt ; cnt++ ) //2�� �о��� ���԰��� 1�� ���� �н��� �ǵ��� �Ѵ�!!
		g_minfo[ g_knownpath[ g_u16path_cnt - cnt - 1 ].u8pos ].bit.u16weight = 0xffff - cnt;

	min_weight = 0xffff;
	g_u16path_cnt = 0;
	cur_pos = 0;
	mouse_dir = 0;
	memset( ( void * )g_knownpath , 0x00 , sizeof( g_knownpath ) );

	while( 1 ) //g_knownpath�� �ٽ� �����.
	{
		for( i = 0 ; i < 4 ; i++ )
		{
			if( !( ( g_u16map[ cur_pos ] & 0x0f ) & g_u16wall_t[ i ] ) )
			{
				next_weight = g_minfo[ cur_pos + g_int32move_t[ i ] ].bit.u16weight;
				if( next_weight < min_weight )
				{
					min_weight = next_weight;
					next_pos = cur_pos + g_int32move_t[ i ];
					turn_dir = i;
					next_dir = ( turn_dir + 4 - mouse_dir ) & 0x03;					
				}
			}
		}

		g_knownpath[ g_u16path_cnt ].u8pos = cur_pos;
		g_knownpath[ g_u16path_cnt ].u4m_dir = mouse_dir;
		g_knownpath[ g_u16path_cnt ].u4t_dir = next_dir;

		cur_pos = next_pos;
		mouse_dir = turn_dir;

		g_u16path_cnt++;
		if( g_u16path_cnt > MAX_MAZE - 1 )
		{
			vfdprintf("err4");
			MOTOR_OFF;
			while( 1 );
		}		

		if( next_pos == goal_pos )
		{
			g_knownpath[ g_u16path_cnt ].u8pos = goal_pos;
			g_knownpath[ g_u16path_cnt ].u4m_dir = mouse_dir;
			g_knownpath[ g_u16path_cnt ].u4t_dir = STRAIGHT;

			g_u16path_cnt++;
			
			break;
		}		
	
	}

	g_u16mouse_dir = mouse_dir;
	g_u16mouse_pos = goal_pos + g_int32move_t[ mouse_dir ];	

	all_case_path_make_func( FASTRUN );	

	memset( ( void * )g_u16map , 0x00 , sizeof( g_u16map ) );
	memset( ( void * )g_u16map_backup0 , 0x00 , sizeof( g_u16map_backup0 ) );

	SpiLoadRom( ( Uint16 )( MAZE_MAP_BACKUP0_PAGE ) , 0x00 ,  ( Uint16 )( MAZE_MAX_BLOCK ) , ( Uint16 * )g_u16map_backup0 ); //����� �̷� �ҷ� ��.

	memmove( ( void * )g_u16map , ( const void * )g_u16map_backup0 , sizeof( g_u16map ) ); //�ְ�� ������ ������ ����!!

//	g_flag.goal = GO_GOAL; //flag ����.

}

static void all_case_path_make_func( mouse_run_type_e type )
{
	Uint16 i , cnt , temp_cnt;

	Uint16 state[ 4 ];
	Uint16 diag_dir[ 4 ];
	Uint16 diag_pos[ 4 ];

	Uint16 block_cnt;
	Uint16 last_path = OFF;

	turn_class_e yet_tstate;
	mouse_run_state_e yet_mhead;

	i = cnt = 0;
	a_emhead = STRAIGHT_STATE;
	a_eturn_state = BACKTURN;

	while( 1 )
	{
		if( i + 3 < g_u16path_cnt )
		{
			state[ 3 ] = g_knownpath[ i + 3 ].u4t_dir;
			state[ 2 ] = g_knownpath[ i + 2 ].u4t_dir;
			state[ 1 ] = g_knownpath[ i + 1 ].u4t_dir;
			state[ 0 ] = g_knownpath[ i ].u4t_dir;

			diag_pos[ 3 ] = g_knownpath[ i + 3 ].u8pos;
			diag_pos[ 2 ] = g_knownpath[ i + 2 ].u8pos;
			diag_pos[ 1 ] = g_knownpath[ i + 1 ].u8pos;
			diag_pos[ 0 ] = g_knownpath[ i ].u8pos;

			diag_dir[ 3 ] = g_knownpath[ i + 3 ].u4m_dir;
			diag_dir[ 2 ] = g_knownpath[ i + 2 ].u4m_dir;
			diag_dir[ 1 ] = g_knownpath[ i + 1 ].u4m_dir;
			diag_dir[ 0 ] = g_knownpath[ i ].u4m_dir;
		}
		else if( i + 2 < g_u16path_cnt )
		{
			state[ 3 ] = GARBAGE_DATA;
			state[ 2 ] = g_knownpath[ i + 2 ].u4t_dir;
			state[ 1 ] = g_knownpath[ i + 1 ].u4t_dir;
			state[ 0 ] = g_knownpath[ i ].u4t_dir;

			diag_pos[ 2 ] = g_knownpath[ i + 2 ].u8pos;
			diag_pos[ 1 ] = g_knownpath[ i + 1 ].u8pos;
			diag_pos[ 0 ] = g_knownpath[ i ].u8pos;

			diag_dir[ 2 ] = g_knownpath[ i + 2 ].u4m_dir;
			diag_dir[ 1 ] = g_knownpath[ i + 1 ].u4m_dir;
			diag_dir[ 0 ] = g_knownpath[ i ].u4m_dir;

			if( ( state[ 2 ] != F ) && ( g_u16path_cnt == i + 3 ) && ( a_emhead == STRAIGHT_STATE ) )			
				last_path = ON;
		}
		else if( i + 1 < g_u16path_cnt )
		{
			state[ 3 ] = GARBAGE_DATA;
			state[ 2 ] = GARBAGE_DATA;
			state[ 1 ] = g_knownpath[ i + 1 ].u4t_dir;
			state[ 0 ] = g_knownpath[ i ].u4t_dir;

			diag_pos[ 1 ] = g_knownpath[ i + 1 ].u8pos;
			diag_pos[ 0 ] = g_knownpath[ i ].u8pos;

			diag_dir[ 1 ] = g_knownpath[ i + 1 ].u4m_dir;
			diag_dir[ 0 ] = g_knownpath[ i ].u4m_dir;		

			if( g_u16path_cnt == i + 2 )
				last_path = ON;
		}
		else if( i < g_u16path_cnt )
		{
			state[ 3 ] = GARBAGE_DATA;
			state[ 2 ] = GARBAGE_DATA;
			state[ 1 ] = GARBAGE_DATA;
			state[ 0 ] = g_knownpath[ i ].u4t_dir;

			diag_pos[ 0 ] = g_knownpath[ i ].u8pos;
			diag_dir[ 0 ] = g_knownpath[ i ].u4m_dir;

			if( g_u16path_cnt == i + 1 )
				last_path = ON;
		}		
		else
		{
			g_u16path_cnt = cnt;
			g_knownpath[ cnt ].u8state = ( type == FASTRUN ) ? STRAIGHT : LAST_PATH; //Ž���̸�  last path�̰� 2���̸� goal���� ���� ����.

			break;
		}

		yet_mhead = a_emhead;
		yet_tstate = a_eturn_state;

		if( a_emhead == STRAIGHT_STATE ) //������ ������ ���.
		{
			if( ( state[ 0 ] == F ) && ( last_path == OFF ) )
			{
				if( state[ 1 ] == R )
				{
					if( state[ 2 ] == R )
					{
						if( state[ 3 ] == F )					 //FRRF -> R180
						{
							a_emhead = STRAIGHT_STATE;
							a_eturn_state = R180;
						}
						else									 //FRR -> R135IN
						{
							a_emhead = DIAGONAL_STATE;
							a_eturn_state = R135IN;
						}

						i += 3;
					}
					else if( state[ 2 ] == F )					 //FRF -> R90
					{
						a_emhead = STRAIGHT_STATE;
						a_eturn_state = R90;

						i += 2;
					}
					else										 //FR -> R45IN
					{
						a_emhead = DIAGONAL_STATE;
						a_eturn_state = R45IN;

						i += 2;
					}
					
				}
				else if( state[ 1 ] == L )
				{
					if( state[ 2 ] == L )
					{
						if( state[ 3 ] == F )					 //FLLF -> L180
						{
							a_emhead = STRAIGHT_STATE;
							a_eturn_state = L180;
						}
						else									 //FLL -> L135IN
						{
							a_emhead = DIAGONAL_STATE;
							a_eturn_state = L135IN;
						}

						i += 3;
					}
					else if( state[ 2 ] == F )					 //FLF -> L90
					{
						a_emhead = STRAIGHT_STATE;
						a_eturn_state = L90;

						i += 2;
					}
					else										 //FL -> L45IN
					{
						a_emhead = DIAGONAL_STATE;
						a_eturn_state = L45IN;

						i += 2;
					}
					
				}
				else if( state[ 1 ] == F )
				{
					a_emhead = STRAIGHT_STATE;
					a_eturn_state = STRAIGHT;

					if( state[ 2 ] == F )
					{
						if( state[ 3 ] == F )					 //FFFF
						{
							i += 3;
							block_cnt = 3;
						}
						else									 //FFF
						{
							i += 2;
							block_cnt = 2;
						}
					}
					else 										 //FF
					{
						i += 1;
						block_cnt = 1;
					}
					
				}
				else											 //F
				{
					a_emhead = STRAIGHT_STATE;
					a_eturn_state = STRAIGHT;

					i += 1;
					block_cnt = 1;

					temp_cnt = ( yet_tstate == STRAIGHT ) ? cnt - 1 : cnt;

					g_knownpath[ temp_cnt ].u8pos = diag_pos[ 0 ];
					g_knownpath[ temp_cnt ].u4m_dir = diag_dir[ 0 ];
				}

				if( a_eturn_state != STRAIGHT ) //�н� ã�� �� �Ѻ� ���� ����.
				{
					if( yet_tstate == STRAIGHT )
					{
						g_knownpath[ cnt - 1 ].u8cnt++;
						g_knownpath[ cnt - 1 ].u4m_dir = diag_dir[ 0 ];

						if( g_knownpath[ cnt - 1 ].u8pos != 0x00 )
							g_knownpath[ cnt - 1 ].u8pos = diag_pos[ 0 ];
					}
					else
					{
						g_knownpath[ cnt ].u8state = STRAIGHT;
						g_knownpath[ cnt ].u8pos = diag_pos[ 0 ];
						g_knownpath[ cnt ].u4m_dir = diag_dir[ 0 ];
						g_knownpath[ cnt++ ].u8cnt = 1;
					}
				}
				
			}
			else //not match!!
			{
				a_emhead = STRAIGHT_STATE;
				a_eturn_state = NOT_MATCH;

				if( state[ 0 ] == F ) //last path!!
				{
					a_eturn_state = STRAIGHT;
					block_cnt = 1;
				}

				i += 1;
			}			
			
		}
		else //diagonal
		{
			if( state[ 0 ] == R )
			{
				if( state[ 1 ] == R )
				{
					if( state[ 2 ] == F ) 									//RRF -> R135OUT
					{
						a_emhead = STRAIGHT_STATE;
						a_eturn_state = R135OUT;
					}
					else													//RR
					{
						if( last_path == OFF )		 						//RD90
						{
							a_emhead = DIAGONAL_STATE;
							a_eturn_state = RD90;
						}
						else						 						//RCBR135OUT
						{
							a_emhead = STRAIGHT_STATE;
							a_eturn_state = RCBR135OUT;
						}
					}
				
					i += 2;
					
				}
				else if( state[ 1 ] == F )							 		//RF -> R45OUT
				{
					a_emhead = STRAIGHT_STATE;
					a_eturn_state = R45OUT;

					i += 1;
				}
				else if( state[ 1 ] == L )							 		//RL -> RDRUN
				{
					a_emhead = DIAGONAL_STATE;
					a_eturn_state = RDRUN;

					i += 1;					
				}
				else												 		//R -> RCBR45OUT
				{
					a_emhead = STRAIGHT_STATE;
					a_eturn_state = RCBR45OUT;

					i += 1;					
				}
				
			}
			else if( state[ 0 ] == L )
			{
				if( state[ 1 ] == L )
				{
					if( ( state[ 2 ] == F ) || 
						( state[ 2 ] == B && g_flag.search_end == ON ) )	//LLF -> L135OUT
					{
						a_emhead = STRAIGHT_STATE;
						a_eturn_state = L135OUT;
					}					
					else											 		//LL
					{
						if( last_path == OFF )								//LD90
						{
							a_emhead = DIAGONAL_STATE;
							a_eturn_state = LD90;
						}
						else												//LCBR135OUT
						{
							a_emhead = STRAIGHT_STATE;
							a_eturn_state = LCBR135OUT;
						}
					}

					i += 2;
					
				}			
				else if( ( state[ 1 ] == F ) ||
						 ( state[ 1 ] == B && g_flag.search_end == ON ) ) 	//LF -> L45OUT
				{
					a_emhead = STRAIGHT_STATE;
					a_eturn_state = L45OUT;

					i += 1;
				}
				else if( state[ 1 ] == R )									 //LR -> LDRUN
				{
					a_emhead = DIAGONAL_STATE;
					a_eturn_state = LDRUN;

					i += 1;
				}
				else														 //L -> LCBR45OUT
				{
					a_emhead = STRAIGHT_STATE;
					a_eturn_state = LCBR45OUT;

					i += 1;
				}
				
			}
			else;			
			
		}

		if( a_eturn_state == STRAIGHT ) //���� ����.
		{
			if( yet_tstate == STRAIGHT )
			{
				cnt--;
				g_knownpath[ cnt ].u8cnt += block_cnt;

				if( g_knownpath[ cnt ].u8pos != 0x00 )
					g_knownpath[ cnt ].u8pos = diag_pos[ 0 ];
			}
			else
			{
				g_knownpath[ cnt ].u8state = STRAIGHT;
				g_knownpath[ cnt ].u8cnt = block_cnt;
				g_knownpath[ cnt ].u8pos = diag_pos[ 0 ];
			}
		}
		else //�밢 ����.
		{
			g_knownpath[ cnt ].u8state = a_eturn_state;

			if( yet_mhead == STRAIGHT_STATE )
			{
				if( a_eturn_state == NOT_MATCH )
				{
					if( state[ 0 ] == L )		g_knownpath[ cnt ].u8state = L90;
					else if( state[ 0 ] == R )	g_knownpath[ cnt ].u8state = R90;
					else if( state[ 0 ] == B )	g_knownpath[ cnt ].u8state = BACKTURN;
					else;

					g_knownpath[ cnt ].u8pos = diag_pos[ 0 ];
					g_knownpath[ cnt ].u4m_dir = diag_dir[ 0 ];
				}
				else //�밢 ���� , 90
				{
					if( ( a_eturn_state == R180 ) || ( a_eturn_state == L180 ) ||
						( a_eturn_state == R135IN ) || ( a_eturn_state == L135IN ) )
					{
						g_knownpath[ cnt ].u8pos = diag_pos[ 2 ];
						g_knownpath[ cnt ].u4m_dir = diag_dir[ 2 ];
					}
					else //R45IN , L45IN , R90 , L90
					{
						g_knownpath[ cnt ].u8pos = diag_pos[ 1 ];
						g_knownpath[ cnt ].u4m_dir = diag_dir[ 1 ];
					}
				}
				
			}
			else //�밢 ���� , Ż��.
			{
				if( ( a_eturn_state == RDRUN ) || ( a_eturn_state == LDRUN ) )
				{
					if( ( yet_tstate == RDRUN ) || ( yet_tstate == LDRUN ) )
						g_knownpath[ --cnt ].u8cnt++;
					else
						g_knownpath[ cnt ].u8cnt = 1;
				}

				g_knownpath[ cnt ].u8pos = diag_pos[ 1 ];
				g_knownpath[ cnt ].u4m_dir = diag_dir[ 1 ];
			}
			
		}

		cnt++;
		
	}
	
}

void known_path_printf( void )
{
	int32 i;
	const char *pstr[] = { "STRA" , "R90" , "BACK" , "L90" , "R180" , "L180" , "R135IN" , "L135IN" , 
						   "R45IN" , "L45IN" , "R135OUT" , "L135OUT" , "R45OUT" , "L45OUT" , "RD90" , "LD90" , 
						   "RCBR45OUT" , "LCBR45OUT" , "RCBR135OUT" , "LCBR135OUT" , "RDRUN" , "LDRUN" , "NOT" };

	for( i = 0 ; i < g_u16path_cnt ; i++ )
		SCIa_Printf("%3u 0x%02x %10s %2u %2u %2u \n" , 
		( Uint16 )i , 
		g_knownpath[ i ].u8pos , 
		pstr[ g_knownpath[ i ].u8state ] , 
		g_knownpath[ i ].u8cnt , 
		g_knownpath[ i ].u4m_dir , 
		g_knownpath[ i ].u4t_dir );
}

static int32 _memory_allocation( void **ptr , size_t size )
{
	if( ptr && *ptr )
		_end_algo_vari_init();

	*ptr = ( void * )far_malloc( size ); //1kbyte ��뷮 ���� �Ҵ�.
	if( *ptr == NULL )
	{
		MOTOR_OFF;
	
		vfdprintf("err2");
		while( 1 );
	}

	memset( ( void * )*ptr , 0x00 , size ); //�ʱ�ȭ.

	return 0;		
}

static void _memory_free( void **ptr )
{
	if( ptr && *ptr )
	{
		far_free( ( void * )*ptr );
		*ptr = NULL;
	}
}

static int32 _insert_data_in_queue( queue_t *pque , Uint16 pos , Uint16 weight ) //queue by linked list.
{
	list_t *pnew = NULL;
	list_t *pcur = NULL;
	list_t *pnext = NULL;

	_memory_allocation( ( void ** )&pnew , sizeof( list_t ) );

	pnew->u16pos = pos;
	pnew->u16weight = weight;
	pnew->plink = NULL;

	if( pque->phead == NULL ) //������ �� ���.
	{
		pque->phead = pnew;
		pque->prear = pnew;	
	}
	else
	{
		pcur = pque->phead;
		pnext = pque->phead->plink;

		while( pnext ) //���԰� ���� �켱������ ���ĵǰ� ����.
		{
			if( pnext->u16weight > weight )
				break;		

			pcur = pnext;
			pnext = pnext->plink;
		}

		if( pnext == NULL )
		{
			pque->prear->plink = pnew;
			pque->prear = pnew;
		}
		else
		{
			pnew->plink = pnext;
			pcur->plink = pnew;
		}
		
	}

	return 0;
	
}


static int32 _desert_data_by_queue( queue_t *pque ) //queue by linked list.
{
	int32 ret = -1;
	list_t *pdel = pque->phead;

	if( pdel )
	{
		ret = ( int32 )pdel->u16pos;

		pque->phead = pque->phead->plink;
		if( pque->phead == NULL )
			pque->prear = NULL;

		_memory_free( ( void ** )&pdel );
	}

	return ret;
}

static void _delete_all_data_in_queue( queue_t *pque )
{
	while( _desert_data_by_queue( pque ) != -1 );
}


