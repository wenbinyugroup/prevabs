SUBROUTINE TitlePrint(file_unit, title)

    INTEGER,INTENT(IN)        ::file_unit
    CHARACTER(*),INTENT(IN)   ::title
    
    WRITE(file_unit,*)
    WRITE(file_unit,'(1x,100A)') title
    WRITE(file_unit,*)'========================================================'
    WRITE(file_unit,*)
    
END SUBROUTINE TitlePrint

FUNCTION FileOpen(file_unit,file_name,sta_type,rw_type,error,form)

    LOGICAL                   ::FileOpen
    INTEGER,INTENT(IN)        ::file_unit
    CHARACTER(*),INTENT(IN)   ::file_name
    CHARACTER(*),INTENT(IN)   ::sta_type
    CHARACTER(*),INTENT(IN)   ::rw_type
    CHARACTER(*),INTENT(OUT)::error
    
    CHARACTER(*),OPTIONAL,INTENT(IN)   ::form
    
    error=''
    FileOpen=.FALSE.
    
    IF(PRESENT(form)) THEN
        OPEN (UNIT=file_unit, FORM=form,file=file_name,STATUS=sta_type,ACTION = rw_type,IOSTAT=in_stat)
    ELSE
        OPEN (UNIT=file_unit, file=file_name,STATUS=sta_type,ACTION = rw_type,IOSTAT=in_stat)
    ENDIF
    
    IF (in_stat/=0) THEN
      IF(rw_type=='READ') error='Cannot open the file '//TRIM(file_name)//' for reading!'
      IF(rw_type=='WRITE')error='Cannot open the file '//TRIM(file_name)//' for writing!'
      IF(rw_type=='READWRITE')error='Cannot open the file '//TRIM(file_name)//' for reading & writing!'
    
    ENDIF
    
    IF (error/='') FileOpen=.TRUE.
    
END FUNCTION FileOpen

!*******************************************************

!*                                                     *

!* Obtain the principal direction and principal values *

!*                                                     *

!******************************************************

SUBROUTINE PrincipalAxes(matrix_in,I22,I33,theta_out)
    INTEGER, PARAMETER:: DBL=SELECTED_REAL_KIND(15)
    REAL(DBL), PARAMETER  :: PI = ATAN(1.0)*4
    REAL(DBL), PARAMETER  :: DEG_2_Rad = PI/180
    REAL(DBL), INTENT(IN) :: matrix_in(2:3,2:3)
    REAL(DBL), INTENT(OUT):: I22,I33   ! Principal values
    REAL(DBL), INTENT(OUT):: theta_out ! Principal axes pitch angle, positive rotating around x_1 direction
    REAL(DBL)             :: delta,m22,m33,m23,temp,cos_2theta,sin_2theta
    
    m22 = matrix_in(2,2)
    m33 = matrix_in(3,3)
    m23 = matrix_in(2,3)
    temp= 0.5d0*(m22-m33)
    delta  = sqrt(temp*temp+m23*m23)
    sin_2theta = m23/delta
    cos_2theta = temp/delta
    
    IF(sin_2theta>=0) THEN
      theta_out=ACOS(cos_2theta)*0.5D0
    ELSE
      theta_out=PI-ACOS(cos_2theta)*0.5D0
    ENDIF
    
    I22 = 0.5d0*(m33+m22)+delta
    I33 = 0.5d0*(m33+m22)-delta
    theta_out=theta_out/DEG_2_Rad

END SUBROUTINE PrincipalAxes

!******************************************************

SUBROUTINE Output(inp_name, mass, area, xm2, xm3, mass_mc, I22, I33, mass_angle, Xg2, Xg3, &
    Aee, Aee_F, Xe2, Xe3, Aee_k, Aee_k_F, Aee_damp, Xe2_k, Xe3_k, ST, ST_F, ST_damp, Sc1, Sc2, &
    stiff_val, stiff_val_F, stiff_val_damp, Ag1, Bk1, Ck2, Dk3, thermal_I, cte, temperature, &
    NT, NT_F, Vlasov_I, damping_I, curved_I, Timoshenko_I, trapeze_I, error)

    interface
        FUNCTION FileOpen (file_unit,file_name,sta_type,rw_type,error,form)
            LOGICAL                   ::FileOpen
            INTEGER,INTENT(IN)        ::file_unit
            CHARACTER(*),INTENT(IN)   ::file_name
            CHARACTER(*),INTENT(IN)   ::sta_type
            CHARACTER(*),INTENT(IN)   ::rw_type
            CHARACTER(*),INTENT(OUT)::error
            CHARACTER(*),OPTIONAL,INTENT(IN)   ::form
        end function
    end interface
    external TitlePrint

    INTEGER, PARAMETER:: DBL=SELECTED_REAL_KIND(15)
    INTEGER, PARAMETER:: NE_1D=4       ! number of classical strains for 1D problem: 4.
    INTEGER,PARAMETER:: OUT =40
    INTEGER,PARAMETER        :: CHAR_LEN=256

    CHARACTER(CHAR_LEN)      :: inp_name
    CHARACTER(CHAR_LEN+1)    :: out_name
    REAL(DBL),INTENT(INOUT) :: I22,I33,mass_angle,ST_F(6,6)
    REAL(DBL),INTENT(IN)  :: mass(6,6),area, xm2, xm3,mass_mc(6,6),Xg2,Xg3 ! mass properties and centroid
    REAL(DBL),INTENT(IN)  :: Aee(NE_1D,NE_1D),Aee_F(NE_1D,NE_1D),Xe2,Xe3              !Output for classical modeling
    REAL(DBL),INTENT(IN)  :: Aee_k(NE_1D,NE_1D),Aee_k_F(NE_1D,NE_1D),Xe2_k,Xe3_k      !Output for curved/twisted modeling
    REAL(DBL),INTENT(IN)  :: ST(6,6),Sc1,Sc2  !Output for Timoshenko modeling
    REAL(DBL),INTENT(IN)  :: stiff_val(5,5),stiff_val_F(5,5)     !Output for Vlasov modeling
    REAL(DBL),INTENT(IN)  :: Ag1(4,4),Bk1(4,4),Ck2(4,4),Dk3(4,4) !Outputs for Trapeze effect
    REAL(DBL),INTENT(IN)  :: Aee_damp(NE_1D,NE_1D),ST_damp(6,6),stiff_val_damp(5,5)

    INTEGER,  INTENT(IN):: thermal_I
    REAL(DBL),INTENT(IN)   :: cte(nmate,6)
    REAL(DBL),INTENT(IN):: temperature(nnode)
    REAL(DBL),INTENT(IN)  :: NT(4),NT_F(4)
    INTEGER,  INTENT(IN)   :: Timoshenko_I,curved_I,trapeze_I,Vlasov_I, damping_I
    CHARACTER(300), INTENT(INOUT)::error         ! a character variable to hold the error message

    INTEGER:: i,j
    REAL(DBL)::I11,radius_gyration
    REAL(DBL),PARAMETER:: TOLERANCE = 1.0D-15
    error = ''
    
    
    ! Output the cross-sectional properties
    !====================================================
    out_name=TRIM(inp_name) // ".K"
    IF(FileOpen(OUT,  out_name,'REPLACE','WRITE',error))	 GOTO 9999
    
    !==============================
    ! Output the mass matrix
    !==============================
    CALL TitlePrint(OUT, 'The 6X6 Mass Matrix')
    WRITE(OUT,'(1x,6ES20.10)')(mass(i,:),i=1,6)
    
    CALL TitlePrint(OUT, 'The Mass Center of the Cross-Section')
    WRITE(OUT,'(1x,2ES20.10)')xm2,xm3
    
    !--------------------------------------------------------------------
    ! output the mass matrix at the mass center if the mass center is
    ! not at the origin
    !---------------------------------------------------------------------
    IF(ABS(xm2)>TOLERANCE.OR.ABS(xm3)>TOLERANCE) THEN
    
    	CALL TitlePrint(OUT, 'The 6X6 Mass Matrix at the Mass Center')
    	WRITE(OUT,'(1x, 6ES20.12)')(mass_mc(i,:),i=1,6)
    
    ENDIF
    
    !--------------------------------------------------------------------
    ! output the mass properties at mass center with respect to the
    ! principal inertial axes
    !---------------------------------------------------------------------
    I11=I22+I33
    CALL TitlePrint(OUT, 'The Mass Properties with respect to Principal Inertial Axes')
    WRITE(OUT,'(1x,A40, ES20.10)')'Mass per unit span                     =', mass_mc(1,1)
    WRITE(OUT,'(1x,A40, ES20.10)')'Mass moment of inertia i11            =', I11
    WRITE(OUT,'(1x,A40, ES20.10)')'Principal mass moments of inertia i22  =', I22
    WRITE(OUT,'(1x,A40, ES20.10)')'Principal mass moments of inertia i33  =', I33
    
    
    IF(ABS(mass_angle)>100000*TOLERANCE) THEN
    	WRITE(OUT,*)'The principal inertial axes rotated from user coordinate system by ', mass_angle, &
    	            & 'degrees about the positive direction of x1 axis.'
    ELSE
    	WRITE(OUT,*)'The user coordinate axes are the principal inertial axes.'
    ENDIF
    radius_gyration=sqrt((I22+I33)/mass_mc(1,1))
    WRITE(OUT,'(1x,A40, ES20.10)')'The mass-weighted radius of gyration   =', radius_gyration
    
    
    CALL TitlePrint(OUT, 'The Geometric Center of the Cross-Section')
    WRITE(OUT,'(1x,2ES20.10)')Xg2,Xg3
    
    CALL TitlePrint(OUT, 'The Area of the Cross-Section')
    WRITE(OUT,'(1x,A6, ES20.10)')'Area =',area
    
    
    
    CALL TitlePrint(OUT, 'Classical Stiffness Matrix (1-extension; 2-twist; 3,4-bending)')
    WRITE(OUT,'(1x,4ES20.10)')(Aee(i,:),i=1,NE_1D)
    
    CALL TitlePrint(OUT, 'Classical Compliance Matrix (1-extension; 2-twist; 3,4-bending)')
    WRITE(OUT,'(1x,4ES20.10)')(Aee_F(i,:),i=1,NE_1D)
    
    if(damping_I==1)then
        CALL TitlePrint(OUT, 'Classical Damping Matrix (1-extension; 2-twist; 3,4-bending)')
        WRITE(OUT,'(1x,4ES20.10)')(Aee_damp(i,:),i=1,NE_1D)
    end if
    
    IF(thermal_I==3)THEN
    
    	CALL TitlePrint(OUT, 'Nonmechanical Stress Resultants for Classical Model (1-extension; 2-twist; 3,4-bending)')
    	WRITE(OUT,'(1x,4ES20.10)')(NT(i),i=1,NE_1D)
    
    	CALL TitlePrint(OUT, 'Thermal Strains for Classical Model (1-extension; 2-twist; 3,4-bending)')
    	WRITE(OUT,'(1x,4ES20.10)')(NT_F(i),i=1,NE_1D)
    
    ENDIF
    
    CALL TitlePrint(OUT, 'The Tension Center of the Cross-Section')
    WRITE(OUT,'(1x,2ES20.10)')Xe2, Xe3
    WRITE(OUT,*)
    
    ! Below I22, I33 becomes bending compliances, mass_angle is the angle for principal bending axes
    WRITE(OUT,'(1x,A33 ES20.10)')'The extension stiffness EA       =',1.0D0/(Aee_F(1,1)-Xe2*Aee_F(1,4)+Xe3*Aee_F(1,3))
    WRITE(OUT,'(1x,A33 ES20.10)')'The torsional stiffness GJ       =',1.0D0/Aee_F(2,2)
    
    IF(abs(Aee_F(3,4))>TOLERANCE*100000*Aee_F(3,3)) THEN
       CALL PrincipalAxes(Aee_F(3:4,3:4),I22,I33,mass_angle)
    ELSE
       I22=Aee_F(3,3)
       I33=Aee_F(4,4)
       mass_angle=0.0D0
    ENDIF
    WRITE(OUT,'(1x,A33, ES20.10)')'Principal bending stiffness EI22 =', 1.0D0/I22
    WRITE(OUT,'(1x,A33, ES20.10)')'Principal bending stiffness EI33 =', 1.0D0/I33
    
    
    IF(ABS(mass_angle)>TOLERANCE) THEN
    	WRITE(OUT,*)'The principal bending axes rotated from the user coordinate system by ', mass_angle, &
    	            & 'degrees about the positive direction of x1 axis.'
    ELSE
    	WRITE(OUT,*)'The user coordinate axes are the principal bending axes.'
    ENDIF
    
    
    
    IF(curved_I==1) THEN
        CALL TitlePrint(OUT, 'Classical Stiffness Matrix (correct up to the 2nd order) (1-extension; 2-twist; 3,4-bending)')
    	WRITE(OUT,'(1x,4ES20.10)')(Aee_k(i,:),i=1,NE_1D)
    
    	CALL TitlePrint(OUT, 'Classical Compliance Matrix (correct up to the 2nd order) (1-extension; 2-twist; 3,4-bending)')
    	WRITE(OUT,'(1x,4ES20.10)')(Aee_k_F(i,:),i=1,NE_1D)
    
        CALL TitlePrint(OUT, 'The Tension Center of the Cross Section, Corrected by Initial Curvature/Twist')
        WRITE(OUT,'(1x, 2ES20.10)')Xe2_k, Xe3_k
        WRITE(OUT,*)
    
        WRITE(OUT,'(1x,A33, ES20.10)')'The extension stiffness EA       =',&
            &                   1.0D0/(Aee_k_F(1,1)-Xe2_k*Aee_k_F(1,4)+Xe3_k*Aee_k_F(1,3))
    
        WRITE(OUT,'(1x,A33, ES20.10)')'The torsional stiffness GJ       =',1.0D0/Aee_k_F(2,2)
    
        IF(abs(Aee_k_F(3,4))>TOLERANCE*100000*Aee_k_F(3,3)) THEN
            CALL PrincipalAxes(Aee_k_F(3:4,3:4),I22,I33,mass_angle)
        ELSE
            I22=Aee_k_F(3,3)
            I33=Aee_k_F(4,4)
            mass_angle=0.0D0
        ENDIF
    
        WRITE(OUT,'(1x,A33, ES20.10)')'Principal bending stiffness EI22 =', 1.0D0/I22
        WRITE(OUT,'(1x,A33, ES20.10)')'Principal bending stiffness EI33 =', 1.0D0/I33
    
    
        IF(ABS(mass_angle)>TOLERANCE) THEN
            WRITE(OUT,*)'The principal bending axes rotated from user coordinate system by ', mass_angle, &
    	         & 'degrees about the positive direction of x1 axis.'
        ELSE
            WRITE(OUT,*)'The user coordinate axes are the principal bending axes.'
        ENDIF
    
    ENDIF
    
    IF(Timoshenko_I==1) THEN
        CALL TitlePrint(OUT, 'Timoshenko Stiffness Matrix (1-extension; 2,3-shear, 4-twist; 5,6-bending)')
        WRITE(OUT,'(1x,6ES20.10)')(ST(i,:),i=1,6)
    
        CALL TitlePrint(OUT, 'Timoshenko Compliance Matrix (1-extension; 2,3-shear, 4-twist; 5,6-bending)')
        WRITE(OUT,'(1x,6ES20.10)')(ST_F(i,:),i=1,6)
    
        if(damping_I==1)then
            CALL TitlePrint(OUT, 'Timoshenko Damping Matrix (1-extension; 2,3-shear, 4-twist; 5,6-bending)')
            WRITE(OUT,'(1x,6ES20.10)')(ST_damp(i,:),i=1,6)
        end if
    
        CALL TitlePrint(OUT, 'The Generalized Shear Center of the Cross-Section in the User Coordinate System')
        WRITE(OUT,'(1x,2ES20.10)')Sc1, Sc2
        WRITE(OUT,*)
    
        ST_F(2,2)=ST_F(2,2)-ST_F(2,4)*Sc2
        ST_F(2,3)=ST_F(2,3)+ST_F(2,4)*Sc1
        ST_F(3,2)=ST_F(2,3)
        ST_F(3,3)=ST_F(3,3)+ST_F(3,4)*Sc1
    
        IF(abs(ST_F(2,3))>TOLERANCE*100000*ST_F(2,2)) THEN
            CALL PrincipalAxes(ST_F(2:3,2:3),I22,I33,mass_angle)
        ELSE
            I22=ST_F(2,2)
            I33=ST_F(3,3)
            mass_angle=0.0D0
        ENDIF
    
        WRITE(OUT,'(1x,A32, ES20.10)')'Principal shear stiffness GA22 =', 1.0D0/I22
        WRITE(OUT,'(1x,A32, ES20.10)')'Principal shear stiffness GA33 =', 1.0D0/I33
    
        IF(ABS(mass_angle)>TOLERANCE) THEN
            WRITE(OUT,*)'The principal shear axes rotated from user coordinate system by ', mass_angle, &
    	            & 'degrees about the positive direction of x1 axis.'
        ELSE
            WRITE(OUT,*)'The user coordinate axes are the principal shear axes.'
        ENDIF
    ENDIF
    
    
    IF(Vlasov_I==1) THEN
    	CALL TitlePrint(OUT, 'Vlasov Stiffness Matrix (1-extension; 2-twist; 3,4-bending; 5-twist rate)')
    	WRITE(OUT,'(1x,5ES20.10)')(stiff_val(i,:),i=1,5)
    
    	CALL TitlePrint(OUT, 'Vlasov Compliance Matrix (1-extension; 2-twist; 3,4-bending; 5-twist rate)')
    	WRITE(OUT,'(1x,5ES20.10)')(stiff_val_F(i,:),i=1,5)
    
        if(damping_I==1)then
            CALL TitlePrint(OUT, 'Vlasov Damping Matrix (1-extension; 2-twist; 3,4-bending; 5-twist rate)')
            WRITE(OUT,'(1x,5ES20.10)')(stiff_val_damp(i,:),i=1,5)
        end if
    ENDIF
    
    
    IF(trapeze_I==1) THEN
    	CALL TitlePrint(OUT, 'Trapeze Effects Related Matrices')
    	CALL TitlePrint(OUT, 'Ag1--Ag1--Ag1--Ag1')
    	WRITE(OUT,'(1x,4ES20.10)')(Ag1(i,:),i=1,4)
    
    	CALL TitlePrint(OUT, 'Bk1--Bk1--Bk1--Bk1')
    	WRITE(OUT,'(1x,4ES20.10)')(Bk1(i,:),i=1,4)
    
    	CALL TitlePrint(OUT, 'Ck2--Ck2--Ck2--Ck2')
    	WRITE(OUT,'(1x,4ES20.10)')(Ck2(i,:),i=1,4)
    
    	CALL TitlePrint(OUT, 'Dk3--Dk3--Dk3--Dk3')
    	WRITE(OUT,'(1x,4ES20.10)')(Dk3(i,:),i=1,4)
    ENDIF
    
    
    WRITE(*,*)
    WRITE(*,*)'Cross-sectional properties can be found in  "',TRIM(out_name),'"'
    
    CLOSE(OUT)
    
    
    
    9999 RETURN
    
    END SUBROUTINE Output


