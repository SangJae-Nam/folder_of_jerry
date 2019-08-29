//###########################################################################
//
// FILE		: motor.c
//
// TITLE		: micromouse H2 motor source file.
//
// Author	: leejaeseong
//
// Company	: Hertz
//
//###########################################################################
// $Release Date: 2011.10.02 $
//###########################################################################

#include "DSP2833x_Device.h"     // DSP2833x Headerfile Include File
#include "DSP2833x_Examples.h"   // DSP2833x Examples Include File

#pragma CODE_SECTION(motor_pid_ISR, "ramfuncs");

//#define MOTOR_DEBUG

///////////////////////////////////////////////    motor information   ///////////////////////////////////////////////////

//#define WHEEL_RADIUS			25
//#define GEAR_RATIO				4.83
#define M_PI					3.141592653589

//#define SAMPLE_FRQ				0.0005		//500us
#define SAMPLE_FRQ_MS			0.5			//500ms


//PULSE_TO_D = ( WHEEL_RADIUS * M_PI ) / ( encoder_pulse * 4 ) / GEAR_RATIO 
//( 25 * M_PI ) / 2048 / 4.83
#define PULSE_TO_D				0.0079398591505467972552696045


//PULSE_TO_V = ( WHEEL_RADIUS * M_PI ) / ( encoder_pulse * 4 ) / GEAR_RATIO / SAMPLE_FRQ
//( 25 * M_PI ) / 2048 / 4.83 / 0.0005
#define PULSE_TO_V  			15.879718301093594510539209


////////////////////////////////////////////       PID information       ///////////////////////////////////////////////////

#define MAX_INTEGRAL_VAL		( float32 )5.0

#define MAX_PID_OUT				8950.0
#define MIN_PID_OUT				-8950.0
#define PWM_CONVERT				0.3333333333333//PWM���ļ� �ִ밪(EPWM.c) / MAX_PID_OUT => 3000 / 9000(���� ���)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int a_int16stop_cnt = 0;

/* motor variable struct initialize func */
static void _init_motor_variable( motor_vari_t *pm )
{
	memset( ( void * )pm , 0x00 , sizeof( motor_vari_t ) );

	//���� �ް� kp�� ���� �ʿ� -> �߻� �ϴ� �� ���ƾ� ��!!
	pm->fp32kp = ( float32 )3.25;
	pm->fp32ki = ( float32 )0.00001;
	pm->fp32kd = ( float32 )3.65;
	
	pm->int32accel = 5;	
}


void motor_vari_init( void )
{
	MOTOR_ON;

	_init_motor_variable( &g_rmotor );
	_init_motor_variable( &g_lmotor );
}

void diffvel_to_remaindist_cpt( volatile float32 cur_vel , volatile float32 tar_vel , volatile int32 acc , volatile float32 *decel_dist )
{
	//| v^2 - v0^2 | = 2as �� s�� ���� ������ �ٲ� �Լ�.

	cur_vel /= ( float32 )( 1000.0 );
	tar_vel /= ( float32 )( 1000.0 ); //���� �����÷ο츦 �����ϱ� ���� 1000���� ���� �� ���� ���� �ٽ� 1000�� ���Ѵ�!!

	*decel_dist = ( fabs( ( cur_vel * cur_vel ) - ( tar_vel * tar_vel ) ) / ( ( float32 )( 2.0 ) * ( float32 )( acc ) ) ) * ( float32 )( 1000.0 );
}

void dist_to_maxvel_cpt( volatile float32 dist , volatile float32 minus_dist , volatile float32 cur_vel , volatile int32 acc , volatile float32 *max_vel , volatile float32 limit_vel  )
{
	//| v^2 - v0^2 | = 2as �� v�� ���� ������ �ٲ� �Լ�.

	dist -= minus_dist;

	dist /= ( float32 )( 2000.0 );		 //�Ÿ��� ���ݸ� ��ȿ�ؾ� �ϹǷ� 2�� ������.
	cur_vel /= ( float32 )( 1000.0 );

	*max_vel = sqrt( ( cur_vel * cur_vel ) + ( float32 )( 2.0 ) * ( float32 )( acc ) * dist ) * ( float32 )( 1000.0 );

	if( limit_vel < *max_vel )							*max_vel = limit_vel;
	else if( ( float32 )g_int32turn_vel > *max_vel )	*max_vel = ( float32 )g_int32turn_vel;
	else;
}

void move_end( volatile float32 r_dist , volatile float32 r_vel , volatile float32 l_dist , volatile float32 l_vel , volatile int32 acc ) //dist�Ÿ���ŭ vel�ӵ��� �̵��� �� acc���ӵ��� ����
{
	StopCpuTimer2(); // ���� ����� �� Ÿ�̸Ӱ� �ɸ��� �ȵǹǷ�...

	g_fp32left_pos = ( float32 )0.0;
	g_fp32right_pos = ( float32 )0.0;
	memset( ( void * )&g_pos , 0x00 , sizeof( pos_vari_t ) );	

	g_rmotor.int32accel = g_lmotor.int32accel = acc; //���� ���� ���ӵ� ����

	g_rmotor.u16stop_flag = OFF;
	g_lmotor.u16stop_flag = OFF;

	if( r_dist == ( float32 )0.0 )	g_rmotor.u16stop_flag = ON;
	if( l_dist == ( float32 )0.0 )	g_lmotor.u16stop_flag = ON;

	diffvel_to_remaindist_cpt( r_vel , ( float32 )0.0 , acc , &g_rmotor.fp32decel_distance );  //���� �Ÿ� ���
	diffvel_to_remaindist_cpt( l_vel , ( float32 )0.0 , acc , &g_lmotor.fp32decel_distance );

	if( ( g_rmotor.fp32decel_distance * ( float32 )2.0 ) > fabs( r_dist ) )
		g_rmotor.fp32decel_distance = fabs( r_dist ) * ( float32 )0.5;
	if( ( g_lmotor.fp32decel_distance * ( float32 )2.0 ) > fabs( l_dist ) )
		g_lmotor.fp32decel_distance = fabs( l_dist ) * ( float32 )0.5;	

	g_rmotor.fp32user_distacne = r_dist; //��� �Ÿ�
	g_lmotor.fp32user_distacne = l_dist;

	g_rmotor.fp32user_vel = r_vel; //��� �ӵ�
	g_lmotor.fp32user_vel = l_vel;

	g_rmotor.fp32decel_vel = 0.0; //Ÿ�� �ӵ�
	g_lmotor.fp32decel_vel = 0.0;

	g_rmotor.fp32distance_sum = 0.0;
	g_lmotor.fp32distance_sum = 0.0;	

	g_rmotor.fp32err_distance = g_rmotor.fp32user_distacne - g_rmotor.fp32distance_sum; //���� �Ÿ�
	g_lmotor.fp32err_distance = g_lmotor.fp32user_distacne - g_lmotor.fp32distance_sum;

	g_flag.move_state = RUN;

	g_int32tick = 0;

#ifdef MOTOR_DEBUG
	SCIa_Printf("\n\nleft_decel_dist : %10lf right_decel_dist : %10lf\n\n\n" , g_lmotor.fp32decel_distance , g_rmotor.fp32decel_distance );
#endif		

	StartCpuTimer2(); //motor interrupt start
	
}

void move_to_move( volatile float dist , volatile float tar_vel , volatile float32 dec_vel , volatile int32 acc ) //dist�Ÿ��� tar_vel�� ������ �� ���� �������� dev_vel�� ���� �Ѵ�.
{
	volatile float32 big_vel = 0.0;
	volatile float32 small_vel = 0.0;

	volatile float32 cur_vel = 0.0;	
	volatile float32 acc_vel = 0.0;
	
	volatile float32 m_dist = 0.0;
	volatile float32 dec_dist = 0.0;

	volatile float32 *p_vel = NULL;

	StopCpuTimer2();// ���� ����� �� Ÿ�̸Ӱ� �ɸ��� �ȵǹǷ�...

	g_rmotor.int32accel = g_lmotor.int32accel = acc;	 //�̵��� �� �ӵ��� ���� ���ӵ� ����

	g_rmotor.u16stop_flag = OFF;
	g_lmotor.u16stop_flag = OFF;

	cur_vel = ( g_rmotor.fp32cur_vel_avr + g_lmotor.fp32cur_vel_avr ) * 0.5;  //���� �ӵ� ���ϱ�.

	big_vel = MAX( cur_vel , dec_vel );
	small_vel = MIN( cur_vel , dec_vel );
	diffvel_to_remaindist_cpt( cur_vel , dec_vel , acc , &m_dist );  //�ӵ� �ٸ� ���� �Ÿ��� ���.

	if( m_dist > dist ) //������ ������ ��ü �Ÿ����� ū ��� -> �� ��� �ʿ�!!
	{
		dec_dist = dist;  //��� �Ÿ��� ���� ����!!
		dist_to_maxvel_cpt( dist , ( float32 )0.0 , small_vel , acc , &acc_vel , tar_vel );  //���� �Ÿ����� �� �� �ִ� �ְ� �ӵ� ���.

		p_vel = ( cur_vel > dec_vel ) ? &tar_vel : &dec_vel;  //�ְ� �ӵ��� ���� ��.
		*p_vel = acc_vel;
	}
	else
	{
		dist_to_maxvel_cpt( dist , m_dist , big_vel , acc , &acc_vel , tar_vel );
		diffvel_to_remaindist_cpt( acc_vel , dec_vel , acc , &dec_dist );
	}

	g_rmotor.fp32decel_distance = dec_dist;
	g_lmotor.fp32decel_distance = dec_dist;

	g_rmotor.fp32user_distacne = dist;
	g_lmotor.fp32user_distacne = dist;

	g_rmotor.fp32user_vel = tar_vel;
	g_lmotor.fp32user_vel = tar_vel;

	g_rmotor.fp32decel_vel = dec_vel;
	g_lmotor.fp32decel_vel = dec_vel;

	g_rmotor.fp32distance_sum = 0.0;
	g_lmotor.fp32distance_sum = 0.0;		

	g_rmotor.fp32err_distance = g_rmotor.fp32user_distacne - g_rmotor.fp32distance_sum; //���� �Ÿ�
	g_lmotor.fp32err_distance = g_lmotor.fp32user_distacne - g_lmotor.fp32distance_sum;

	g_flag.move_state = RUN;

	g_int32tick = 0;

#ifdef MOTOR_DEBUG
	SCIa_Printf("\n\ndecel_dist : %10lf acc_vel : %10lf\n\n\n" , g_rmotor.fp32decel_distance , acc_vel );
#endif	

	StartCpuTimer2();  //motor interrupt start

}

void handle_ad_make( volatile float32 acc_rate , volatile float32 dec_rate ) //handle ���� ���� �Լ� -> �����ǿ� ���� �ϼӵ� ������ ���
{
	g_fp32han_accdiff = ( acc_rate / CENTER_POSITION );
	g_fp32han_decdiff = ( dec_rate / CENTER_POSITION );

	g_fp32han_accref = ( float32 )1.0 - acc_rate;
	g_fp32han_decref = ( float32 )1.0 - dec_rate;
}


interrupt void motor_pid_ISR( void )
{
	g_int32tick++;
	g_int32menu_cnt++;
	g_int32pid_ISR_cnt++;
	g_int32edge_diff_cnt++;

	g_u32time_cnt++;

	/* qep value sampling */
	g_rmotor.u16qep_sample = ( Uint16 )RQepRegs.QPOSCNT;
	g_lmotor.u16qep_sample = ( Uint16 )LQepRegs.QPOSCNT;

	/* qep reset */
	RQepRegs.QEPCTL.bit.SWI = 1;
	LQepRegs.QEPCTL.bit.SWI = 1;

	/* qep counter value signed */
	g_rmotor.int16qep_value = g_rmotor.u16qep_sample > 2048 ? 4097 - ( int16 )( g_rmotor.u16qep_sample ) : -( int16 )( g_rmotor.u16qep_sample );
	g_lmotor.int16qep_value = g_lmotor.u16qep_sample > 2048 ? ( int16 )( g_lmotor.u16qep_sample ) - 4097 : ( int16 )( g_lmotor.u16qep_sample );

	/* distance compute */
	g_rmotor.fp32tick_distance = ( float32 )g_rmotor.int16qep_value * ( float32 )PULSE_TO_D;	
	g_rmotor.fp32distance_sum += g_rmotor.fp32tick_distance;
	g_rmotor.fp32err_distance = g_rmotor.fp32user_distacne - g_rmotor.fp32distance_sum;

	g_lmotor.fp32tick_distance = ( float32 )g_lmotor.int16qep_value * ( float32 )PULSE_TO_D;	
	g_lmotor.fp32distance_sum += g_lmotor.fp32tick_distance;
	g_lmotor.fp32err_distance = g_lmotor.fp32user_distacne - g_lmotor.fp32distance_sum;

	g_fp32err_dist += ( g_rmotor.fp32tick_distance + g_lmotor.fp32tick_distance );
	if( g_prss->fp32position < ( float32 )512.0 || g_plss->fp32position < ( float32 )512.0 )
		g_fp32err_dist = 0.0;

	/* average velocity compute */
	g_rmotor.fp32current_vel[ 3 ] = g_rmotor.fp32current_vel[ 2 ];
	g_rmotor.fp32current_vel[ 2 ] = g_rmotor.fp32current_vel[ 1 ];
	g_rmotor.fp32current_vel[ 1 ] = g_rmotor.fp32current_vel[ 0 ];
	g_rmotor.fp32current_vel[ 0 ] = ( float32 )g_rmotor.int16qep_value * ( float32 )PULSE_TO_V;	
	g_rmotor.fp32cur_vel_avr = ( g_rmotor.fp32current_vel[ 0 ] + g_rmotor.fp32current_vel[ 1 ] + g_rmotor.fp32current_vel[ 2 ] + g_rmotor.fp32current_vel[ 3 ] ) * 0.25;

	g_lmotor.fp32current_vel[ 3 ] = g_lmotor.fp32current_vel[ 2 ];
	g_lmotor.fp32current_vel[ 2 ] = g_lmotor.fp32current_vel[ 1 ];
	g_lmotor.fp32current_vel[ 1 ] = g_lmotor.fp32current_vel[ 0 ];
	g_lmotor.fp32current_vel[ 0 ] = ( float32 )g_lmotor.int16qep_value * ( float32 )PULSE_TO_V;	
	g_lmotor.fp32cur_vel_avr = ( g_lmotor.fp32current_vel[ 0 ] + g_lmotor.fp32current_vel[ 1 ] + g_lmotor.fp32current_vel[ 2 ] + g_lmotor.fp32current_vel[ 3 ] ) * 0.25;

	/* decelation a point of time flag */
	if( ( g_rmotor.u16stop_flag == OFF ) && g_rmotor.fp32decel_distance >= ( float32 )fabs( ( double )( g_rmotor.fp32err_distance ) ) ) //���� �� �� �ִ� ���� ������ ������ ���.
	{
		g_rmotor.fp32user_vel = g_rmotor.fp32decel_vel; //user_vel�� ���� �ӵ��� ��ȯ	
		g_rmotor.u16stop_flag = ( g_rmotor.fp32decel_vel == ( float32 )0.0 ) ? ON : AVOID;
	}		

	if( ( g_lmotor.u16stop_flag == OFF ) && g_lmotor.fp32decel_distance >= ( float32 )fabs( ( double )( g_lmotor.fp32err_distance ) ) ) //���� �� �� �ִ� ���� ������ ������ ���.
	{
		g_lmotor.fp32user_vel = g_lmotor.fp32decel_vel; //user_vel�� ���� �ӵ��� ��ȯ
		g_lmotor.u16stop_flag = ( g_lmotor.fp32decel_vel == ( float32 )0.0 ) ? ON : AVOID;
	}		

	/* accel & decel compute */
	if( g_rmotor.fp32user_vel > g_rmotor.fp32next_vel )
	{
		g_rmotor.fp32next_vel += ( ( float32 )fabs( ( double )( g_rmotor.int32accel ) ) * ( float32 )SAMPLE_FRQ_MS );
		if( g_rmotor.fp32user_vel < g_rmotor.fp32next_vel )
			g_rmotor.fp32next_vel = g_rmotor.fp32user_vel;	
	}
	else if( g_rmotor.fp32user_vel < g_rmotor.fp32next_vel )
	{
		g_rmotor.fp32next_vel -= ( ( float32 )fabs( ( double )( g_rmotor.int32accel ) ) * ( float32 )SAMPLE_FRQ_MS );
		if( g_rmotor.fp32user_vel > g_rmotor.fp32next_vel )
			g_rmotor.fp32next_vel = g_rmotor.fp32user_vel;	
	}
	else;

	if( g_lmotor.fp32user_vel > g_lmotor.fp32next_vel )
	{
		g_lmotor.fp32next_vel += ( ( float32 )fabs( ( double )( g_lmotor.int32accel ) ) * ( float32 )SAMPLE_FRQ_MS );
		if( g_lmotor.fp32user_vel < g_lmotor.fp32next_vel )
			g_lmotor.fp32next_vel = g_lmotor.fp32user_vel;	
	}
	else if( g_lmotor.fp32user_vel < g_lmotor.fp32next_vel )
	{
		g_lmotor.fp32next_vel -= ( ( float32 )fabs( ( double )( g_lmotor.int32accel ) ) * ( float32 )SAMPLE_FRQ_MS );
		if( g_lmotor.fp32user_vel > g_lmotor.fp32next_vel )
			g_lmotor.fp32next_vel = g_lmotor.fp32user_vel;	
	}
	else;

	if( ( g_rmotor.u16stop_flag == ON ) && ( g_lmotor.u16stop_flag == ON ) && 
		( g_rmotor.fp32cur_vel_avr == ( float32 )0.0 ) && ( g_lmotor.fp32cur_vel_avr == ( float32 )0.0 ) )  //���� ���� �÷���.
	{
		a_int16stop_cnt++;
		if( a_int16stop_cnt > 3 )
		{
			a_int16stop_cnt = 0;
			g_flag.move_state = STOP;
		}
	}
	else
	{
		a_int16stop_cnt = 0;
		g_flag.move_state = RUN;
	}

	position_pid_compute_func();

	/* motor PID compute */
	g_rmotor.fp32err_vel_sum -= g_rmotor.fp32err_vel[ 3 ];
	g_rmotor.fp32err_vel[ 3 ] = g_rmotor.fp32err_vel[ 2 ];
	g_rmotor.fp32err_vel[ 2 ] = g_rmotor.fp32err_vel[ 1 ];
	g_rmotor.fp32err_vel[ 1 ] = g_rmotor.fp32err_vel[ 0 ];
	g_rmotor.fp32err_vel[ 0 ] = ( g_rmotor.fp32next_vel * g_fp32right_handle ) - g_rmotor.fp32cur_vel_avr;
	g_rmotor.fp32err_vel_sum += g_rmotor.fp32err_vel[ 0 ];

	g_rmotor.fp32proportion_val = g_rmotor.fp32kp * g_rmotor.fp32err_vel[ 0 ];
	g_rmotor.fp32integral_val = g_rmotor.fp32ki * g_rmotor.fp32err_vel_sum;
	g_rmotor.fp32differential_val = g_rmotor.fp32kd * ( ( g_rmotor.fp32err_vel[ 0 ] - g_rmotor.fp32err_vel[ 3 ] ) + ( ( float32 )3.0 * ( g_rmotor.fp32err_vel[ 1 ] - g_rmotor.fp32err_vel[ 2 ] ) ) );

	if( g_rmotor.fp32integral_val > MAX_INTEGRAL_VAL )			g_rmotor.fp32integral_val = MAX_INTEGRAL_VAL;
	else if( g_rmotor.fp32integral_val < -MAX_INTEGRAL_VAL )	g_rmotor.fp32integral_val = -MAX_INTEGRAL_VAL;
	else;
	
	g_rmotor.fp32PID_output += ( g_rmotor.fp32proportion_val + g_rmotor.fp32integral_val + g_rmotor.fp32differential_val );

	g_lmotor.fp32err_vel_sum -= g_lmotor.fp32err_vel[ 3 ];
	g_lmotor.fp32err_vel[ 3 ] = g_lmotor.fp32err_vel[ 2 ];
	g_lmotor.fp32err_vel[ 2 ] = g_lmotor.fp32err_vel[ 1 ];
	g_lmotor.fp32err_vel[ 1 ] = g_lmotor.fp32err_vel[ 0 ];
	g_lmotor.fp32err_vel[ 0 ] = ( g_lmotor.fp32next_vel * g_fp32left_handle ) - g_lmotor.fp32cur_vel_avr;
	g_lmotor.fp32err_vel_sum += g_lmotor.fp32err_vel[0];

	g_lmotor.fp32proportion_val = g_lmotor.fp32kp * g_lmotor.fp32err_vel[ 0 ];
	g_lmotor.fp32integral_val = g_lmotor.fp32ki * g_lmotor.fp32err_vel_sum;
	g_lmotor.fp32differential_val = g_lmotor.fp32kd * ( ( g_lmotor.fp32err_vel[ 0 ] - g_lmotor.fp32err_vel[ 3 ] ) + ( ( float32 )3.0 * ( g_lmotor.fp32err_vel[ 1 ] - g_lmotor.fp32err_vel[ 2 ] ) ) );

	if( g_lmotor.fp32integral_val > MAX_INTEGRAL_VAL )			g_lmotor.fp32integral_val = MAX_INTEGRAL_VAL;
	else if( g_lmotor.fp32integral_val < -MAX_INTEGRAL_VAL )	g_lmotor.fp32integral_val = -MAX_INTEGRAL_VAL;
	else;
	
	g_lmotor.fp32PID_output += ( g_lmotor.fp32proportion_val + g_lmotor.fp32integral_val + g_lmotor.fp32differential_val );

	if( g_flag.start == ON )
	{
		/* PID -> PWM */
		if( g_rmotor.fp32PID_output > 0.0 )
		{
			if( g_rmotor.fp32PID_output > MAX_PID_OUT )	g_rmotor.fp32PID_output = MAX_PID_OUT;

			RightPwmRegs.AQCTLA.bit.ZRO = AQ_CLEAR;
			RightPwmRegs.AQCTLB.bit.ZRO = AQ_SET;

			RightPwmRegs.CMPB = ( Uint16 )( g_rmotor.fp32PID_output * PWM_CONVERT );
		}
		else
		{
			if( g_rmotor.fp32PID_output < MIN_PID_OUT )	g_rmotor.fp32PID_output = MIN_PID_OUT;

			RightPwmRegs.AQCTLA.bit.ZRO = AQ_SET;
			RightPwmRegs.AQCTLB.bit.ZRO = AQ_CLEAR;		

			RightPwmRegs.CMPA.half.CMPA = ( Uint16 )( g_rmotor.fp32PID_output * PWM_CONVERT * ( -1.0 ) );
		}
		
		if( g_lmotor.fp32PID_output > 0.0  )
		{
			if( g_lmotor.fp32PID_output > MAX_PID_OUT )	g_lmotor.fp32PID_output = MAX_PID_OUT;
			
			LeftPwmRegs.AQCTLA.bit.ZRO = AQ_CLEAR;
			LeftPwmRegs.AQCTLB.bit.ZRO = AQ_SET;
			
			LeftPwmRegs.CMPB = ( Uint16 )( g_lmotor.fp32PID_output * PWM_CONVERT );
		}
		else
		{
			if( g_lmotor.fp32PID_output < MIN_PID_OUT )	g_lmotor.fp32PID_output = MIN_PID_OUT;

			LeftPwmRegs.AQCTLA.bit.ZRO = AQ_SET;
			LeftPwmRegs.AQCTLB.bit.ZRO = AQ_CLEAR;

			LeftPwmRegs.CMPA.half.CMPA = ( Uint16 )( g_lmotor.fp32PID_output * PWM_CONVERT * ( -1.0 ) );
		}
		
	}
	else
	{
		RightPwmRegs.AQCTLA.bit.ZRO = AQ_CLEAR;
		RightPwmRegs.AQCTLB.bit.ZRO = AQ_SET;

		LeftPwmRegs.AQCTLA.bit.ZRO = AQ_CLEAR;
		LeftPwmRegs.AQCTLB.bit.ZRO = AQ_SET;
		
		RightPwmRegs.CMPB = LeftPwmRegs.CMPB = 0;  	
	}

	StartCpuTimer0();  //sensor interrupt start!!
	
}


