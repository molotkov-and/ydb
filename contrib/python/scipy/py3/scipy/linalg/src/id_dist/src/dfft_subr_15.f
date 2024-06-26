      SUBROUTINE DSINT (N,X,WSAVE)
	IMPLICIT DOUBLE PRECISION (A-H,O-Z)
      DIMENSION       X(*)       ,WSAVE(*)
      NP1 = N+1
      IW1 = N/2+1
      IW2 = IW1+NP1
      IW3 = IW2+NP1
      CALL DSINT1(N,X,WSAVE,WSAVE(IW1),WSAVE(IW2),WSAVE(IW3))
      RETURN
      END

      SUBROUTINE DSINTI (N,WSAVE)
	IMPLICIT DOUBLE PRECISION (A-H,O-Z)
      DIMENSION       WSAVE(*)
      DATA PI /3.1415926535897932384626433832795028D0/
      IF (N .LE. 1) RETURN
      NS2 = N/2
      NP1 = N+1
      DT = PI/DBLE(NP1)
      DO 101 K=1,NS2
         WSAVE(K) = 2.0D0*DSIN(K*DT)
  101 CONTINUE
      CALL DFFTI (NP1,WSAVE(NS2+1))
      RETURN
      END
