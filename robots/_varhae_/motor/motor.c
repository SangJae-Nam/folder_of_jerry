//###########################################################################
//
// FILE		: motor.c
//
// TITLE		: _varhae_ Tracer motor source file.
//
// Author	: leejaeseong
//
// Company	: Hertz
//
//###########################################################################
// $Release Date: 2009.10.02 $
//###########################################################################

#include "DSP2833x_Device.h"     // DSP2833x Headerfile Include File
#include "DSP2833x_Examples.h"   // DSP2833x Examples Include File

#pragma CODE_SECTION(motor_pid_ISR, "ramfuncs");

///////////////////////////////////////////////    motor information   ///////////////////////////////////////////////////

//#define WHEEL_RADIUS			36
//#define Gear_Ratio 				3.35
#define M_PI					3.141592653589

//#define SAMPLE_FRQ				0.00025			//250us
#define SAMPLE_FRQ_MS			0.25			//���ӵ��� ���� ������ ���� ���� 250ms �� ���

//PULSE_TO_D = (WHEEL_RADIUS * M_PI) / (encoder_pulse * 4) / geer_ratio 
//(36 * M_PI) / 2048 / 3.35
#define PULSE_TO_D				0.016484569660

//PULSE_TO_V = (WHEEL_RADIUS * M_PI) / (encoder_pulse * 4) / geer_ratio / SAMPLE_FRQ
//(36 * M_PI) / 2048 / 3.35 / 0.00025
#define PULSE_TO_V  			65.93827864344

////////////////////////////////////////////       PID information       ///////////////////////////////////////////////////

// 9000���� ���Ƿ� ��� ������� ��� ��ȸ������ 7800mm/s �������� �ö󰡴°� Ȯ��-> ���� �����϶��� ���� �� �� �÷��� �� ��...
#define MAX_PID_OUT				8950.0
#define MIN_PID_OUT				-8950.0
#define PWM_CONVERT				0.3333333333333//PWM���ļ� �ִ밪(EPWM.c) / MAX_PID_OUT => 3000 / 9000(���� ���)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//���� �߸� ���� 0.2 ���� �̹Ƿ� 1.5���� 0.2�� ���̴� 1.3 �̴�.
//���� �� �Ÿ��� 200�� �ɶ����� 1.3�� ����߷��� �ϹǷ� X * 200 = 1.3 �̵ȴ�.
//#define DOWN_KP				( float32 )( 0.00725 )
#define	DOWN_KP				( float32 )( 0.007 )  // 0.1
//#define	DOWN_KP				( float32 )( 0.0065 )  // 0.2

#define DOWN_KD				( float32 )( 0.005 )	//3.4
//#define DOWN_KD				( float32 )( 0.01 )		//2.4

///////////////////////////////////////////        jerk control            /////////////////////////////////////////////////////

 //jerk time.
 //T = ( ( 60 * S / x ) ^ 1/3 ) s

 //x�� ���� ������ ��ġ��...
 //x = ( ( 60 * S ) / T^3 ) m/s^3

 //�κ��� ����Ϸ��� mm/s^3���� ���ľ� �ȴ�.( �Ÿ��� mm������ ����ϹǷ�... )
 //x = ( ( 6 * S ) / ( 2.5 * ( 0.00025 )^2 ) ) mm/s^3
 //S�� �� ���ͷ�Ʈ �� �Ÿ� * qep value -> fp32tick_distance = int16qep_value * PULSE_TO_D;

 //��� ����� ��ġ�� 9600 * S �� �����µ� ���ӵ��� ���� ������ ���� ���� �ð��� us�� �ƴ϶� ms �̹Ƿ� 9.6 * S �� �ȴ�.

 //���� �� ���� S�� 0 �̹Ƿ� �ִ� ���ӵ� 17�϶��� �Ÿ� S = 1/2at^2 �� ���� 0.5 * 17000 * ( 0.00025 )^2 = 0.00053125 �� ���� ó��.

#define JERK_CONTROL

#ifdef JERK_CONTROL
#define JERK_VALUE			( float32 )0.96
#define START_JERK_LIMIT	( float32 )0.00053125
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


float32	am_Rmotor_step = 0.0;
float32	am_Lmotor_step = 0.0;

void debug_force_PWM( str_point_t *pstr ) //������ ���� ������(PWM)
{
	pstr->ppos->fp32current_pos = 0.0;

	StopCpuTimer2();

	GpioDataRegs.GPBSET.bit.GPIO48 = 1; // left
	GpioDataRegs.GPBCLEAR.bit.GPIO49 = 1; // right

	LeftPwmRegs.CMPA.half.CMPA = 400; //���� PWM�� �������		
	LeftPwmRegs.CMPB = 400;

	VFDPrintf(" -TEST- ");

	while( 1 )
	{
		LED_ON;
		DELAY_US( 250000 );
		LED_OFF;
		DELAY_US( 250000 );

		if( !DOWN_SW )
		{
			LeftPwmRegs.CMPA.half.CMPA = 0;
			LeftPwmRegs.CMPB = 0;

			StartCpuTimer2();
			break; //motor ���� �� Ż��...
		}
	}
	
}

/* motor variable struct initialize func */
void init_motor_variable( motor_vari *pm )
{
	memset( ( void * )pm , 0x00 , sizeof( motor_vari ) );

	pm->fp32kp = 0.6; // 0.8
	pm->fp32ki = 0.00002;
	pm->fp32kd = 0.65; // 0.85 //���� ��ũ�� �����ϸ� �ȴ� -> �� ����߷��� �ɵ�. 

	pm->int32accel = 5;	
#ifdef JERK_CONTROL	
	pm->fp32next_acc = START_JERK_LIMIT;
#endif
}

void diffvel_to_remaindist_cpt( volatile float32 cur_vel , volatile float32 tar_vel , volatile int32 acc , volatile float32 *decel_dist )
{
	cur_vel /= ( float32 )( 1000.0 );
	tar_vel /= ( float32 )( 1000.0 );

	*decel_dist = ( fabs( ( cur_vel * cur_vel ) - ( tar_vel * tar_vel ) ) / ( ( float32 )( 2.0 ) * ( float32 )( acc ) ) ) * ( float32 )( 1000.0 );
}

void dist_to_maxvel_cpt( volatile float32 dist , volatile float32 minus_dist , volatile float32 cur_vel , volatile int32 acc , volatile float32 *max_vel )
{
	dist -= minus_dist;

	dist /= ( float32 )( 2000.0 );
	cur_vel /= ( float32 )( 1000.0 );

	*max_vel = sqrt( ( cur_vel * cur_vel ) + ( float32 )( 2.0 ) * ( float32 )( acc ) * dist ) * ( float32 )( 1000.0 );

	if( *max_vel > ( float32 )( g_int32max_speed ) )			*max_vel = ( float32 )( g_int32max_speed ); //�ְ� �ӵ��� ���� �ӵ����� ������ -> ���� �ӵ��� ����
	else if( *max_vel < ( float32 )( g_int32turn_vel ) )		*max_vel = ( float32 )( g_int32turn_vel );  //���� �ӵ��� ���� �� �ӵ����� ������ -> �ϼӵ��� ����
	else;
}

void move_end( volatile float32 dist , volatile float32 vel , volatile int32 acc ) //dist�Ÿ���ŭ vel�ӵ��� �̵��� �� acc���ӵ��� ����
{
	StopCpuTimer2(); // ���� ����� �� Ÿ�̸Ӱ� �ɸ��� �ȵǹǷ�...

	R_motor.int32accel = L_motor.int32accel = acc; //���� ���� ���ӵ� ����

	diffvel_to_remaindist_cpt( vel , ( float32 )0.0 , acc , &R_motor.fp32decel_distance );  //���� �Ÿ� ���
	L_motor.fp32decel_distance = R_motor.fp32decel_distance;

	R_motor.fp32user_distacne = dist; //��� �Ÿ�
	L_motor.fp32user_distacne = dist;

	R_motor.fp32user_vel = vel; //��� �ӵ�
	L_motor.fp32user_vel = vel;

	R_motor.fp32err_distance = R_motor.fp32user_distacne - R_motor.fp32distance_sum; //���� �Ÿ�
	L_motor.fp32err_distance = L_motor.fp32user_distacne - L_motor.fp32distance_sum;

	R_motor.fp32decel_vel = 0.0; //Ÿ�� �ӵ�
	L_motor.fp32decel_vel = 0.0;

	R_motor.u16decel_flag = ON;
	L_motor.u16decel_flag = ON;

	g_Flag.move_state = OFF;

	g_fp32time =  ( float32 )g_int32time_cnt * ( float32 )0.00025; // time count compute	

	StartCpuTimer2(); //motor interrupt start
	
}

void move_to_move( volatile float dist , volatile float dec_dist , volatile float tar_vel , volatile float32 dec_vel , volatile int32 acc ) //dist�Ÿ��� cur_vel�ӵ����� tar_vel�ӵ��� ������ �� dec_vel�ӵ��� acc���ӵ��� ����
{
	StopCpuTimer2();// ���� ����� �� Ÿ�̸Ӱ� �ɸ��� �ȵǹǷ�...

	R_motor.int32accel = L_motor.int32accel = acc;	 //�̵��� �� �ӵ��� ���� ���ӵ� ����

	R_motor.fp32decel_distance = dec_dist;
	L_motor.fp32decel_distance = dec_dist;

	R_motor.fp32user_distacne = dist;
	L_motor.fp32user_distacne = dist;

	if( ( g_Flag.goal_dest == OFF ) || ( g_Flag.goal_dest && !( g_secinfo[ g_int32mark_cnt ].int32dir & STRAIGHT ) ) )
	{
		R_motor.fp32user_vel = tar_vel;
		L_motor.fp32user_vel = tar_vel;
	}

	R_motor.fp32err_distance = R_motor.fp32user_distacne - R_motor.fp32distance_sum;
	L_motor.fp32err_distance = L_motor.fp32user_distacne - L_motor.fp32distance_sum;

	R_motor.fp32decel_vel = dec_vel;
	L_motor.fp32decel_vel = dec_vel;

	R_motor.u16decel_flag = ON;
	L_motor.u16decel_flag = ON;

	g_Flag.move_state = ON;	

	StartCpuTimer2();  //motor interrupt start

}

void handle_ad_make( volatile float32 acc_rate , volatile float32 dec_rate ) //handle ���� ���� �Լ� -> �����ǿ� ���� �ϼӵ� ������ ���
{
	g_fp32han_accstep = ( 1.0 -  acc_rate ) / HANDLE_CENTER;
	g_fp32han_decstep = ( dec_rate - 1.0 ) / HANDLE_CENTER;

	g_fp32han_accmax = acc_rate;
	g_fp32han_decmax = ( 2.0 - dec_rate );
}

static void position_to_vel( void )
{
	volatile float32 limit_vel = ( float32 )( g_int32limit_vel );

	if( g_Flag.line_out || !g_Flag.start_flag || g_Flag.stop_check )
		return; //���� �������� �ƴϸ� Ż��

	if( g_Flag.err )
	{
		LED_ON;
	
		L_motor.int32accel = 10;
		R_motor.int32accel = 10;
	
		L_motor.fp32user_vel = ( float32 )( g_int32turn_vel - 500 );
		R_motor.fp32user_vel = ( float32 )( g_int32turn_vel - 500 );

		return;
	}


	//positon kp �� ctrl -> ������ ���� Ǯ�� ������
	if( g_secinfo[ g_int32mark_cnt ].int32down_flag )  //ª�� ���� �� 
	{
		if( R_motor.fp32decel_distance >= ( float32 )fabs( ( double )( R_motor.fp32err_distance ) ) && 
			L_motor.fp32decel_distance >= ( float32 )fabs( ( double )( L_motor.fp32err_distance ) ) )  //���� ����.
		{
			GREEN_OFF;
			YELLOW_ON;
		}
		else  //���� ����.
		{
			GREEN_ON;
			YELLOW_OFF;
		}	
		
		xkval_ctrl_func( ( KVAL_DOWN | KVAL_KP ) , &g_pos , DOWN_KP , g_secinfo[ g_int32mark_cnt ].fp32kp_down );
	}
	else if( g_secinfo[ g_int32mark_cnt ].int32s44s_flag )  //���� - 45�� - 45�� - ���� ���� ���������� ª�� ������ �ƴ� ���
	{
		if( g_fp32xrun_dist > ( float32 )( g_secinfo[ g_int32mark_cnt ].int32dist - ARBITRATE ) )  //�������ڸ��� kp�� Ǯ�� ���� ������ ���ϹǷ�...
		{		
			LMARK_LED_ON;
			RMARK_LED_ON;
			
			xkval_ctrl_func( ( KVAL_DOWN | KVAL_KP ) , &g_pos , DOWN_KP , g_secinfo[ g_int32mark_cnt ].fp32kp_down );
		}
		else
			xkval_ctrl_func( ( KVAL_UP | KVAL_KP ) , &g_pos , DOWN_KP , g_secinfo[ g_int32mark_cnt ].fp32kp_down );
	}
	else  //kp�� ������� ������
		xkval_ctrl_func( ( KVAL_UP | KVAL_KP ) , &g_pos , DOWN_KP , g_secinfo[ g_int32mark_cnt ].fp32kp_down );


	
	//���� �÷��װ� ���� ��� user_vel�� �ְ� �ӵ��� ������ �� Ż��
	if( g_Flag.speed_up ) 
	{
		BLUE_ON; 					//���ӱ��� �Ķ��� LED	
		g_Flag.straight = ON;
	
		L_motor.fp32user_vel = g_secinfo[ g_int32mark_cnt ].fp32vel;
		R_motor.fp32user_vel = L_motor.fp32user_vel;

		//position kd �� ctrl -> ���� ���� ��鸲 ����.
		if( ( g_Flag.xrun == ON ) && ( g_secinfo[ g_int32mark_cnt ].int32dir & STRAIGHT ) && ( g_secinfo[ g_int32mark_cnt ].int32dist > MID_DIST_LIMIT ) )  //middle ���� �̻��� ����!!
		{	
			if( g_fp32xrun_dist <= ( float32 )( ARBITRATE ) )  //�Ÿ� 200 ���� KD�� �����.
			{
				GREEN_ON;
				xkval_ctrl_func( ( KVAL_DOWN | KVAL_KD ) , &g_pos , DOWN_KD , POS_KD_DOWN );					
			}
			else
			{
				GREEN_OFF;
				xkval_ctrl_func( ( KVAL_UP | KVAL_KD ) , &g_pos , DOWN_KD , POS_KD_DOWN );
			}
		}
		else
			xkval_ctrl_func( ( KVAL_UP | KVAL_KD ) , &g_pos , DOWN_KD , POS_KD_DOWN );

		return;
	}
	
	
	//�ڵ鰪 ���� ����
	if( g_secinfo[ g_int32mark_cnt ].int32dir >= TURN_180 )
	{
		if( ( R_motor.fp32next_vel * g_fp32right_handle ) > limit_vel )		R_motor.fp32next_vel = ( limit_vel / g_fp32right_handle );
		else if( ( L_motor.fp32next_vel * g_fp32left_handle ) > limit_vel )	L_motor.fp32next_vel = ( limit_vel / g_fp32left_handle );
		else;
	}
		
}

interrupt void motor_pid_ISR(void)
{
	g_int32menu_count++;	 //menu switch chattering prevention
	g_int32pid_ISR_cnt++;	 //motor interrupt synchronization 
	g_int32time_cnt++;		 //driving time count

#ifndef MOTOR_TEST
	position_PID_handle();	 //handle compute
	position_to_vel();		 //position to velocity change
#endif

	/* qep value sampling */
	R_motor.u16qep_sample = ( Uint16 )RQepRegs.QPOSCNT;
	L_motor.u16qep_sample = ( Uint16 )LQepRegs.QPOSCNT;

	/* qep reset */
	RQepRegs.QEPCTL.bit.SWI = 1;
	LQepRegs.QEPCTL.bit.SWI = 1;

	/* qep counter value signed */
	R_motor.int16qep_value = R_motor.u16qep_sample > 2048 ? ( int16 )( R_motor.u16qep_sample ) - 4097 : ( int16 )R_motor.u16qep_sample;
	L_motor.int16qep_value = L_motor.u16qep_sample > 2048 ? ( int16 )( L_motor.u16qep_sample ) - 4097 : ( int16 )L_motor.u16qep_sample;	

	/* distance compute */
	R_motor.fp32tick_distance = ( float32 )R_motor.int16qep_value * ( float32 )PULSE_TO_D;	
	R_motor.fp32distance_sum += R_motor.fp32tick_distance;
	R_motor.fp32err_distance = R_motor.fp32user_distacne - R_motor.fp32distance_sum;

	L_motor.fp32tick_distance = ( float32 )L_motor.int16qep_value * ( float32 )PULSE_TO_D;	
	L_motor.fp32distance_sum += L_motor.fp32tick_distance;
	L_motor.fp32err_distance = L_motor.fp32user_distacne - L_motor.fp32distance_sum;

	/* extern distance  make over */
	am_Rmotor_step = R_motor.fp32tick_distance;
	am_Lmotor_step = L_motor.fp32tick_distance;

	g_fp32shift_dist = ( am_Rmotor_step + am_Lmotor_step ) * 0.5;
	
	R_motor.fp32gone_distance += am_Rmotor_step;
	L_motor.fp32gone_distance += am_Lmotor_step;	

	g_rmark.fp32check_dis += am_Rmotor_step;
	g_lmark.fp32check_dis += am_Lmotor_step;

	g_fp32cross_dist += ( ( am_Rmotor_step + am_Lmotor_step ) * 0.5 );

	/* average velocity compute */
	R_motor.fp32current_vel[ 3 ] = R_motor.fp32current_vel[ 2 ];
	R_motor.fp32current_vel[ 2 ] = R_motor.fp32current_vel[ 1 ];
	R_motor.fp32current_vel[ 1 ] = R_motor.fp32current_vel[ 0 ];
	R_motor.fp32current_vel[ 0 ] = ( float32 )R_motor.int16qep_value * ( float32 )PULSE_TO_V;	
	R_motor.fp32cur_vel_avr = ( R_motor.fp32current_vel[ 0 ] + R_motor.fp32current_vel[ 1 ] + R_motor.fp32current_vel[ 2 ] + R_motor.fp32current_vel[ 3 ] ) * 0.25;

	L_motor.fp32current_vel[ 3 ] = L_motor.fp32current_vel[ 2 ];
	L_motor.fp32current_vel[ 2 ] = L_motor.fp32current_vel[ 1 ];
	L_motor.fp32current_vel[ 1 ] = L_motor.fp32current_vel[ 0 ];
	L_motor.fp32current_vel[ 0 ] = ( float32 )L_motor.int16qep_value * ( float32 )PULSE_TO_V;	
	L_motor.fp32cur_vel_avr = ( L_motor.fp32current_vel[ 0 ] + L_motor.fp32current_vel[ 1 ] + L_motor.fp32current_vel[ 2 ] + L_motor.fp32current_vel[ 3 ] ) * 0.25;

	/* decelation a point of time flag */
	if( R_motor.u16decel_flag ) //move_to_move�� move_end�Լ��� ȣ�� �Ǿ��� ���
	{
		if( R_motor.fp32decel_distance >= ( float32 )fabs( ( double )( R_motor.fp32err_distance ) ) ) //���� �� �� �ִ� ���� ������ ������ ���.
		{
			if( g_secinfo[ g_int32mark_cnt ].int32dir & STRAIGHT )
			{
				RED_ON;
				BLUE_OFF;			
			}

#ifdef JERK_CONTROL  //���� ���� �̹Ƿ� ���ӵ��� �����´�.
			R_motor.int32accel = -R_motor.int32accel;
			L_motor.int32accel = -L_motor.int32accel;
#endif
		
			R_motor.fp32user_vel = R_motor.fp32decel_vel;
			L_motor.fp32user_vel = L_motor.fp32decel_vel; //user_vel�� ���� �ӵ��� ��ȯ
			
			R_motor.u16decel_flag = OFF;
			L_motor.u16decel_flag = OFF;

			/* accelation start flag OFF */		
			g_Flag.speed_up = OFF;
			g_Flag.speed_up_start = OFF;
			
			g_err.fp32over_dist = 0.0;
		}
		
	}
	else if( L_motor.u16decel_flag ) //move_to_move�� move_end�Լ��� ȣ�� �Ǿ��� ���
	{
		if( L_motor.fp32decel_distance >= ( float32 )fabs( ( double )( L_motor.fp32err_distance ) ) ) //���� �� �� �ִ� ���� ������ ������ ���.
		{
			if( g_secinfo[ g_int32mark_cnt ].int32dir & STRAIGHT )
			{
				RED_ON;
				BLUE_OFF;			
			}

#ifdef JERK_CONTROL  //���� ���� �̹Ƿ� ���ӵ��� �����´�.
			R_motor.int32accel = -R_motor.int32accel;
			L_motor.int32accel = -L_motor.int32accel;
#endif
			
			R_motor.fp32user_vel = R_motor.fp32decel_vel;
			L_motor.fp32user_vel = L_motor.fp32decel_vel; //user_vel�� ���� �ӵ��� ��ȯ
			
			R_motor.u16decel_flag = OFF;
			L_motor.u16decel_flag = OFF;

			/* accelation start flag OFF */
			g_Flag.speed_up = OFF;
			g_Flag.speed_up_start = OFF;			

			g_err.fp32over_dist = 0.0;			
		}
		
	}
	else;

#ifdef JERK_CONTROL

	/* jerk accel & decel compute */
	if( ( float32 )( R_motor.int32accel ) > R_motor.fp32next_acc )
	{
		R_motor.fp32next_acc += ( JERK_VALUE * R_motor.fp32tick_distance );
		if( ( float32 )R_motor.int32accel < R_motor.fp32next_acc )
			R_motor.fp32next_acc = ( float32 )( R_motor.int32accel );	
	}
	else if( ( float32 )( R_motor.int32accel ) < R_motor.fp32next_acc )	
	{
		R_motor.fp32next_acc -= ( JERK_VALUE * R_motor.fp32tick_distance );
		if( ( float32 )R_motor.int32accel > R_motor.fp32next_acc )
			R_motor.fp32next_acc = ( float32 )( R_motor.int32accel );	
	}
	else;

	if( ( float32 )( L_motor.int32accel ) > L_motor.fp32next_acc )
	{
		L_motor.fp32next_acc += ( JERK_VALUE * L_motor.fp32tick_distance );
		if( ( float32 )L_motor.int32accel < L_motor.fp32next_acc )
			L_motor.fp32next_acc = ( float32 )( L_motor.int32accel );		
	}
	else if( ( float32 )( L_motor.int32accel ) < L_motor.fp32next_acc )	
	{
		L_motor.fp32next_acc -= ( JERK_VALUE * L_motor.fp32tick_distance );
		if( ( float32 )L_motor.int32accel > L_motor.fp32next_acc )
			L_motor.fp32next_acc = ( float32 )( L_motor.int32accel );		
	}
	else;
	
#else

	R_motor.fp32next_acc = ( float32 )R_motor.int32accel;
	L_motor.fp32next_acc = ( float32 )L_motor.int32accel;	
	
#endif

	/* accel & decel compute */
	if( R_motor.fp32user_vel > R_motor.fp32next_vel )
	{
		R_motor.fp32next_vel += ( ( float32 )fabs( ( double )( R_motor.fp32next_acc ) ) * ( float32 )SAMPLE_FRQ_MS );
		if( R_motor.fp32user_vel < R_motor.fp32next_vel )
			R_motor.fp32next_vel = R_motor.fp32user_vel;	
	}
	else if( R_motor.fp32user_vel < R_motor.fp32next_vel )
	{
		R_motor.fp32next_vel -= ( ( float32 )fabs( ( double )( R_motor.fp32next_acc ) ) * ( float32 )SAMPLE_FRQ_MS );
		if( R_motor.fp32user_vel > R_motor.fp32next_vel )
			R_motor.fp32next_vel = R_motor.fp32user_vel;	
	}
	else;
	
	if( L_motor.fp32user_vel > L_motor.fp32next_vel )
	{
		L_motor.fp32next_vel += ( ( float32 )fabs( ( double )( L_motor.fp32next_acc ) ) * ( float32 )SAMPLE_FRQ_MS );
		if( L_motor.fp32user_vel < L_motor.fp32next_vel )
			L_motor.fp32next_vel = L_motor.fp32user_vel;	
	}
	else if( L_motor.fp32user_vel < L_motor.fp32next_vel )
	{
		L_motor.fp32next_vel -= ( ( float32 )fabs( ( double )( L_motor.fp32next_acc ) ) * ( float32 )SAMPLE_FRQ_MS );
		if( L_motor.fp32user_vel > L_motor.fp32next_vel )
			L_motor.fp32next_vel = L_motor.fp32user_vel;	
	}
	else;

	/* motor PID compute */
	R_motor.fp32err_vel_sum -= R_motor.fp32err_vel[ 3 ];
	R_motor.fp32err_vel[ 3 ] = R_motor.fp32err_vel[ 2 ];
	R_motor.fp32err_vel[ 2 ] = R_motor.fp32err_vel[ 1 ];
	R_motor.fp32err_vel[ 1 ] = R_motor.fp32err_vel[ 0 ];
	R_motor.fp32err_vel[ 0 ] = ( R_motor.fp32next_vel * g_fp32right_handle ) - R_motor.fp32cur_vel_avr;
	R_motor.fp32err_vel_sum += R_motor.fp32err_vel[ 0 ];

	R_motor.fp32proportion_val = R_motor.fp32kp * R_motor.fp32err_vel[ 0 ];
	R_motor.fp32integral_val = R_motor.fp32ki * R_motor.fp32err_vel_sum;
	R_motor.fp32differential_val = R_motor.fp32kd * ( ( R_motor.fp32err_vel[ 0 ] - R_motor.fp32err_vel[ 3 ] ) + ( ( float32 )3.0 * ( R_motor.fp32err_vel[ 1 ] - R_motor.fp32err_vel[ 2 ] ) ) );
	R_motor.fp32PID_output += R_motor.fp32proportion_val + R_motor.fp32integral_val + R_motor.fp32differential_val;

	L_motor.fp32err_vel_sum -= L_motor.fp32err_vel[ 3 ];
	L_motor.fp32err_vel[ 3 ] = L_motor.fp32err_vel[ 2 ];
	L_motor.fp32err_vel[ 2 ] = L_motor.fp32err_vel[ 1 ];
	L_motor.fp32err_vel[ 1 ] = L_motor.fp32err_vel[ 0 ];
	L_motor.fp32err_vel[ 0 ] = ( L_motor.fp32next_vel * g_fp32left_handle ) - L_motor.fp32cur_vel_avr;
	L_motor.fp32err_vel_sum += L_motor.fp32err_vel[0];

	L_motor.fp32proportion_val = L_motor.fp32kp * L_motor.fp32err_vel[ 0 ];
	L_motor.fp32integral_val = L_motor.fp32ki * L_motor.fp32err_vel_sum;
	L_motor.fp32differential_val = L_motor.fp32kd * ( ( L_motor.fp32err_vel[ 0 ] - L_motor.fp32err_vel[ 3 ] ) + ( ( float32 )3.0 * ( L_motor.fp32err_vel[ 1 ] - L_motor.fp32err_vel[ 2 ] ) ) );
	L_motor.fp32PID_output += L_motor.fp32proportion_val + L_motor.fp32integral_val + L_motor.fp32differential_val;	

	if( g_Flag.start_flag ) //���� ������ ���
	{
		/* PID -> PWM */
		if( R_motor.fp32PID_output > 0.0 )
		{
			if( R_motor.fp32PID_output > MAX_PID_OUT )	R_motor.fp32PID_output = MAX_PID_OUT;

			GpioDataRegs.GPBCLEAR.bit.GPIO49 = 1; // right

			LeftPwmRegs.CMPA.half.CMPA = ( Uint16 )( R_motor.fp32PID_output * PWM_CONVERT );
		}
		else
		{
			if( R_motor.fp32PID_output < MIN_PID_OUT )	R_motor.fp32PID_output = MIN_PID_OUT;

			GpioDataRegs.GPBSET.bit.GPIO49 = 1; // right

			LeftPwmRegs.CMPA.half.CMPA = ( Uint16 )( R_motor.fp32PID_output * PWM_CONVERT * ( -1.0 ) );
		}
		
		if( L_motor.fp32PID_output > 0.0  )
		{
			if( L_motor.fp32PID_output > MAX_PID_OUT )	L_motor.fp32PID_output = MAX_PID_OUT;

			GpioDataRegs.GPBSET.bit.GPIO48 = 1; // left

			LeftPwmRegs.CMPB = ( Uint16 )( L_motor.fp32PID_output * PWM_CONVERT );

		}
		else
		{
			if( L_motor.fp32PID_output < MIN_PID_OUT )	L_motor.fp32PID_output = MIN_PID_OUT;

			GpioDataRegs.GPBCLEAR.bit.GPIO48 = 1; // left

			LeftPwmRegs.CMPB = ( Uint16 )( L_motor.fp32PID_output * PWM_CONVERT * ( -1.0 ) );
		}

	}
	else //���� �� Ǯ��
	{
		GpioDataRegs.GPBSET.bit.GPIO48 = 1; // left
		GpioDataRegs.GPBCLEAR.bit.GPIO49 = 1; // right	
		
		LeftPwmRegs.CMPA.half.CMPA = 0;  //���� ����
		LeftPwmRegs.CMPB = 0;
	}

	StartCpuTimer0();  //sensor interrupt start
	
}

