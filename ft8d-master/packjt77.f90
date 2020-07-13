module packjt77

! These variables are accessible from outside via "use packjt77":
  parameter (MAXHASH=1000,MAXRECENT=10)
  character (len=13), dimension(1:1024) ::  calls10=''
  character (len=13), dimension(1:4096) ::  calls12=''
  character (len=13), dimension(1:MAXHASH) :: calls22=''
  character (len=13), dimension(1:MAXRECENT) :: recent_calls=''
  character (len=13) :: mycall13=''
  character (len=13) :: dxcall13=''
  integer, dimension(1:MAXHASH) :: ihash22=-1
  integer :: nzhash=0
  integer n28a,n28b

  contains

subroutine hash10(n10,c13)

  character*13 c13

  c13='<...>'
  if(n10.lt.1 .or. n10.gt.1024) return
  if(len(trim(calls10(n10))).gt.0) then
     c13=calls10(n10)
     c13='<'//trim(c13)//'>'
  endif
  return

end subroutine hash10

subroutine hash12(n12,c13)

  character*13 c13

  c13='<...>'
  if(n12.lt.1 .or. n12.gt.4096) return
  if(len(trim(calls12(n12))).gt.0) then
     c13=calls12(n12)
     c13='<'//trim(c13)//'>'
  endif
  return

end subroutine hash12


subroutine hash22(n22,c13)

  character*13 c13

  c13='<...>'
  do i=1,nzhash
     if(ihash22(i).eq.n22) then
        c13=calls22(i)
        c13='<'//trim(c13)//'>'
        go to 900
     endif
  enddo

900 return
end subroutine hash22


integer function ihashcall(c0,m)

  integer*8 n8
  character*13 c0
  character*38 c
  data c/' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/'/

  n8=0
  do i=1,11
     j=index(c,c0(i:i)) - 1
     n8=38*n8 + j
  enddo
  ihashcall=ishft(47055833459_8*n8,m-64)

  return
end function ihashcall

subroutine save_hash_call(c13,n10,n12,n22)

  character*13 c13,cw

  cw=c13
  if(cw(1:1).eq.' ' .or. cw(1:5).eq.'<...>') return
  if(cw(1:1).eq.'<') cw=cw(2:)
  i=index(cw,'>')
  if(i.gt.0) cw(i:)='         '

  if(len(trim(cw)) .lt. 3) return

  n10=ihashcall(cw,10)
  if(n10.ge.1 .and. n10 .le. 1024 .and. cw.ne.mycall13) calls10(n10)=cw

  n12=ihashcall(cw,12)
  if(n12.ge.1 .and. n12 .le. 4096 .and. cw.ne.mycall13) calls12(n12)=cw

  n22=ihashcall(cw,22)
  if(any(ihash22.eq.n22)) then   ! If entry exists, make sure callsign is the most recently received one
    where(ihash22.eq.n22) calls22=cw
    go to 900
  endif

! New entry: move table down, making room for new one at the top
  ihash22(MAXHASH:2:-1)=ihash22(MAXHASH-1:1:-1)

! Add the new entry
  calls22(MAXHASH:2:-1)=calls22(MAXHASH-1:1:-1)
  ihash22(1)=n22
  calls22(1)=cw
  if(nzhash.lt.MAXHASH) nzhash=nzhash+1
900 continue
  return
end subroutine save_hash_call

subroutine pack77(msg0,i3,n3,c77)

  use packjt
  character*37 msg,msg0
  character*18 c18
  character*13 w(19)
  character*77 c77
  integer nw(19)
  integer ntel(3)

  msg=msg0
  if(i3.eq.0 .and. n3.eq.5) go to 5

! Convert msg to upper case; collapse multiple blanks; parse into words.
  call split77(msg,nwords,nw,w)
  i3=-1
  n3=-1
  if(msg(1:3).eq.'CQ ' .or. msg(1:3).eq.'DE ' .or. msg(1:4).eq.'QRZ ') go to 100

! Check 0.1 (DXpedition mode)
  call pack77_01(nwords,w,i3,n3,c77)
  if(i3.ge.0 .or. n3.ge.1) go to 900
! Check 0.2 (EU VHF contest exchange)
  call pack77_02(nwords,w,i3,n3,c77)
  if(i3.ge.0) go to 900

! Check 0.3 and 0.4 (ARRL Field Day exchange)
  call pack77_03(nwords,w,i3,n3,c77)
  if(i3.ge.0) go to 900
  if(nwords.ge.2) go to 100

  ! Check 0.5 (telemetry)
5  i0=index(msg,' ')
  c18=msg(1:i0-1)
  c18=adjustr(c18)
  ntel=-99
  read(c18,1005,err=6) ntel
1005 format(3z6)
  if(ntel(1).ge.2**23) go to 800
6 if(ntel(1).ge.0 .and. ntel(2).ge.0 .and. ntel(3).ge.0) then
     i3=0
     n3=5
     write(c77,1006) ntel,n3,i3
1006 format(b23.23,2b24.24,2b3.3)
     go to 900
  endif

! Check Type 1 (Standard 77-bit message) or Type 2, with optional "/P"
100 call pack77_1(nwords,w,i3,n3,c77)
  if(i3.ge.0) go to 900

! Check Type 3 (ARRL RTTY contest exchange)
  call pack77_3(nwords,w,i3,n3,c77)
  if(i3.ge.0) go to 900

! Check Type 4 (One nonstandard call and one hashed call)
  call pack77_4(nwords,w,i3,n3,c77)
  if(i3.ge.0) go to 900

! It defaults to free text
800 i3=0
  n3=0
  msg(14:)='                        '
  call packtext77(msg(1:13),c77(1:71))
  write(c77(72:77),'(2b3.3)') n3,i3

900 return
end subroutine pack77

subroutine unpack77(c77,nrx,msg,msgcall,msggrid,unpk77_success)
!
! nrx=1 when unpacking a received message
! nrx=0 when unpacking a to-be-transmitted message
! the value of nrx is used to decide when mycall13 or dxcall13 should
! be used in place of a callsign from the hashtable
!
  parameter (NSEC=84)      !Number of ARRL Sections
  parameter (NUSCAN=65)    !Number of US states and Canadian provinces
  parameter (MAXGRID4=32400)
  integer*8 n58
  integer ntel(3)
  character*77 c77
  character*37 msg
  character*13 call_1,call_2,call_3,msgcall
  character*13 mycall13_0,dxcall13_0
  character*11 c11
  character*3 crpt,cntx
  character*3 cmult(NUSCAN)
  character*6 cexch,grid6
  character*4 grid4,cserial,msggrid
  character*3 csec(NSEC)
  character*38 c
  integer hashmy10,hashmy12,hashmy22,hashdx10,hashdx12,hashdx22
  logical unpk28_success,unpk77_success
  logical dxcall13_set,mycall13_set

  data c/' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/'/
  data csec/                                                         &
       "AB ","AK ","AL ","AR ","AZ ","BC ","CO ","CT ","DE ","EB ",  &
       "EMA","ENY","EPA","EWA","GA ","GTA","IA ","ID ","IL ","IN ",  &
       "KS ","KY ","LA ","LAX","MAR","MB ","MDC","ME ","MI ","MN ",  &
       "MO ","MS ","MT ","NC ","ND ","NE ","NFL","NH ","NL ","NLI",  &
       "NM ","NNJ","NNY","NT ","NTX","NV ","OH ","OK ","ONE","ONN",  &
       "ONS","OR ","ORG","PAC","PR ","QC ","RI ","SB ","SC ","SCV",  &
       "SD ","SDG","SF ","SFL","SJV","SK ","SNJ","STX","SV ","TN ",  &
       "UT ","VA ","VI ","VT ","WCF","WI ","WMA","WNY","WPA","WTX",  &
       "WV ","WWA","WY ","DX "/
  data cmult/                                                        &
       "AL ","AK ","AZ ","AR ","CA ","CO ","CT ","DE ","FL ","GA ",  &
       "HI ","ID ","IL ","IN ","IA ","KS ","KY ","LA ","ME ","MD ",  &
       "MA ","MI ","MN ","MS ","MO ","MT ","NE ","NV ","NH ","NJ ",  &
       "NM ","NY ","NC ","ND ","OH ","OK ","OR ","PA ","RI ","SC ",  &
       "SD ","TN ","TX ","UT ","VT ","VA ","WA ","WV ","WI ","WY ",  &
       "NB ","NS ","QC ","ON ","MB ","SK ","AB ","BC ","NWT","NF ",  &
       "LB ","NU ","YT ","PEI","DC "/
  data dxcall13_set/.false./
  data mycall13_set/.false./
  data mycall13_0/''/
  data dxcall13_0/''/

  save hashmy10,hashmy12,hashmy22,hashdx10,hashdx12,hashdx22

  if(mycall13.ne.mycall13_0) then
    if(len(trim(mycall13)).gt.2) then
       mycall13_set=.true.
       mycall13_0=mycall13
       call save_hash_call(mycall13,hashmy10,hashmy12,hashmy22)
    else
       mycall13_set=.false.
    endif
  endif

  if(dxcall13.ne.dxcall13_0) then
    if(len(trim(dxcall13)).gt.2) then
      dxcall13_set=.true.
      dxcall13_0=dxcall13
      hashdx10=ihashcall(dxcall13,10)
      hashdx12=ihashcall(dxcall13,12)
      hashdx22=ihashcall(dxcall13,22)
    endif
  endif
  unpk77_success=.true.

! Check for bad data
  do i=1,77
     if(c77(i:i).ne.'0' .and. c77(i:i).ne.'1') then
        msg='failed unpack'
        unpk77_success=.false.
        return
     endif
  enddo

  read(c77(72:77),'(2b3)') n3,i3
  msg=repeat(' ',37)
  msgcall=repeat(' ',13)
  msggrid=repeat(' ',4)
  if(i3.eq.0 .and. n3.eq.0) then
! 0.0  Free text
     call unpacktext77(c77(1:71),msg(1:13))
     msg(14:)='                        '
     msg=adjustl(msg)

  else if(i3.eq.0 .and. n3.eq.1) then
! 0.1  K1ABC RR73; W9XYZ <KH1/KH7Z> -11   28 28 10 5       71   DXpedition Mode
     read(c77,1010) n28a,n28b,n10,n5
1010 format(2b28,b10,b5)
     irpt=2*n5 - 30
     write(crpt,1012) irpt
1012 format(i3.2)
     if(irpt.ge.0) crpt(1:1)='+'
     call unpack28(n28a,call_1,unpk28_success)
     if(.not.unpk28_success .or. n28a.le.2) unpk77_success=.false.
     call unpack28(n28b,call_2,unpk28_success)
     if(.not.unpk28_success .or. n28b.le.2) unpk77_success=.false.
     call hash10(n10,call_3)
     if(nrx.eq.1     .and. &
        dxcall13_set .and. &
        hashdx10.eq.n10) call_3='<'//trim(dxcall13)//'>'
     if(nrx.eq.0     .and. &
        mycall13_set .and. &
        n10.eq.hashmy10) call_3='<'//trim(mycall13)//'>'
     msg=trim(call_1)//' RR73; '//trim(call_2)//' '//trim(call_3)//' '//crpt
  else if(i3.eq.0 .and. n3.eq.2) then
! 0.2  PA3XYZ/P R 590003 IO91NP           28 1 1 3 12 25   70   EU VHF contest
     read(c77,1020) n28a,ip,ir,irpt,iserial,igrid6
1020 format(b28,2b1,b3,b12,b25)
     call unpack28(n28a,call_1,unpk28_success)
     if(.not.unpk28_success .or. n28a.le.2) unpk77_success=.false.
     nrs=52+irpt
     if(ip.eq.1) call_1=trim(call_1)//'/P'
     write(cexch,1022) nrs,iserial
1022 format(i2,i4.4)
     n=igrid6
     j1=n/(18*10*10*24*24)
     n=n-j1*18*10*10*24*24
     j2=n/(10*10*24*24)
     n=n-j2*10*10*24*24
     j3=n/(10*24*24)
     n=n-j3*10*24*24
     j4=n/(24*24)
     n=n-j4*24*24
     j5=n/24
     j6=n-j5*24
     grid6(1:1)=char(j1+ichar('A'))
     grid6(2:2)=char(j2+ichar('A'))
     grid6(3:3)=char(j3+ichar('0'))
     grid6(4:4)=char(j4+ichar('0'))
     grid6(5:5)=char(j5+ichar('A'))
     grid6(6:6)=char(j6+ichar('A'))
     msg=trim(call_1)//' '//cexch//' '//grid6
     if(ir.eq.1) msg=trim(call_1)//' R '//cexch//' '//grid6

  else if(i3.eq.0 .and. (n3.eq.3 .or. n3.eq.4)) then
! 0.3   WA9XYZ KA1ABC R 16A EMA            28 28 1 4 3 7    71   ARRL Field Day
! 0.4   WA9XYZ KA1ABC R 32A EMA            28 28 1 4 3 7    71   ARRL Field Day
     read(c77,1030) n28a,n28b,ir,intx,nclass,isec
1030 format(2b28,b1,b4,b3,b7)
     if(isec.gt.NSEC .or. isec.lt.1) then
         unpk77_success=.false.
         isec=1
     endif
     call unpack28(n28a,call_1,unpk28_success)
     if(.not.unpk28_success .or. n28a.le.2) unpk77_success=.false.
     call unpack28(n28b,call_2,unpk28_success)
     if(.not.unpk28_success .or. n28b.le.2) unpk77_success=.false.
     ntx=intx+1
     if(n3.eq.4) ntx=ntx+16
     write(cntx(1:2),1032) ntx
1032 format(i2)
     cntx(3:3)=char(ichar('A')+nclass)
     if(ir.eq.0 .and. ntx.lt.10) msg=trim(call_1)//' '//trim(call_2)//     &
          cntx//' '//csec(isec)
     if(ir.eq.1 .and. ntx.lt.10) msg=trim(call_1)//' '//trim(call_2)//     &
          ' R'//cntx//' '//csec(isec)
     if(ir.eq.0 .and. ntx.ge.10) msg=trim(call_1)//' '//trim(call_2)//     &
          ' '//cntx//' '//csec(isec)
     if(ir.eq.1 .and. ntx.ge.10) msg=trim(call_1)//' '//trim(call_2)//     &
          ' R '//cntx//' '//csec(isec)

  else if(i3.eq.0 .and. n3.eq.5) then
! 0.5   0123456789abcdef01                 71               71   Telemetry (18 hex)
     read(c77,1006) ntel
1006 format(b23,2b24)
     write(msg,1007) ntel
1007 format(3z6.6)
     do i=1,18
        if(msg(i:i).ne.'0') exit
        msg(i:i)=' '
     enddo
     msg=adjustl(msg)

  else if(i3.eq.1 .or. i3.eq.2) then
! Type 1 (standard message) or Type 2 ("/P" form for EU VHF contest)
     read(c77,1000) n28a,ipa,n28b,ipb,ir,igrid4,i3
1000 format(2(b28,b1),b1,b15,b3)
     call unpack28(n28a,call_1,unpk28_success)
     if(.not.unpk28_success) unpk77_success=.false.
     call unpack28(n28b,call_2,unpk28_success)
     if(.not.unpk28_success) unpk77_success=.false.
     if(call_1(1:3).eq.'CQ_') call_1(3:3)=' '
     if(index(call_1,'<').le.0) then
        i=index(call_1,' ')
        if(i.ge.4 .and. ipa.eq.1 .and. i3.eq.1) call_1(i:i+1)='/R'
        if(i.ge.4 .and. ipa.eq.1 .and. i3.eq.2) call_1(i:i+1)='/P'
        if(i.ge.4) call add_call_to_recent_calls(call_1)
     endif
     if(index(call_2,'<').le.0) then
        i=index(call_2,' ')
        if(i.ge.4 .and. ipb.eq.1 .and. i3.eq.1) call_2(i:i+1)='/R'
        if(i.ge.4 .and. ipb.eq.1 .and. i3.eq.2) call_2(i:i+1)='/P'
        if(i.ge.4) call add_call_to_recent_calls(call_2)
     endif
     if(igrid4.le.MAXGRID4) then
        n=igrid4
        j1=n/(18*10*10)
        n=n-j1*18*10*10
        j2=n/(10*10)
        n=n-j2*10*10
        j3=n/10
        j4=n-j3*10
        grid4(1:1)=char(j1+ichar('A'))
        grid4(2:2)=char(j2+ichar('A'))
        grid4(3:3)=char(j3+ichar('0'))
        grid4(4:4)=char(j4+ichar('0'))
        if(ir.eq.0) msg=trim(call_1)//' '//trim(call_2)//' '//grid4
        if(ir.eq.1) msg=trim(call_1)//' '//trim(call_2)//' R '//grid4
        if(msg(1:3).eq.'CQ ' .and. ir.eq.1) unpk77_success=.false.
        if(igrid4.ne.32373) msggrid=grid4
     else
        irpt=igrid4-MAXGRID4
        if(irpt.eq.1) msg=trim(call_1)//' '//trim(call_2)
        if(irpt.eq.2) msg=trim(call_1)//' '//trim(call_2)//' RRR'
        if(irpt.eq.3) msg=trim(call_1)//' '//trim(call_2)//' RR73'
        if(irpt.eq.4) msg=trim(call_1)//' '//trim(call_2)//' 73'
        if(irpt.ge.5) then
           write(crpt,'(i3.2)') irpt-35
           if(crpt(1:1).eq.' ') crpt(1:1)='+'
           if(ir.eq.0) msg=trim(call_1)//' '//trim(call_2)//' '//crpt
           if(ir.eq.1) msg=trim(call_1)//' '//trim(call_2)//' R'//crpt
        endif
        if(msg(1:3).eq.'CQ ' .and. irpt.ge.2) unpk77_success=.false.
     endif
     msgcall=trim(call_2)

  else if(i3.eq.3) then
! Type 3: ARRL RTTY Contest
     read(c77,1040) itu,n28a,n28b,ir,irpt,nexch,i3
1040 format(b1,2b28.28,b1,b3.3,b13.13,b3.3)
     write(crpt,1042) irpt+2
1042 format('5',i1,'9')
     nserial=nexch
     imult=-1
     if(nexch.gt.8000) then
        imult=nexch-8000
        nserial=-1
     endif
     call unpack28(n28a,call_1,unpk28_success)
     if(.not.unpk28_success) unpk77_success=.false.
     call unpack28(n28b,call_2,unpk28_success)
     if(.not.unpk28_success) unpk77_success=.false.
     imult=0
     nserial=0
     if(nexch.gt.8000) imult=nexch-8000
     if(nexch.lt.8000) nserial=nexch

     if(imult.ge.1 .and.imult.le.NUSCAN) then
        if(itu.eq.0 .and. ir.eq.0) msg=trim(call_1)//' '//trim(call_2)//             &
             ' '//crpt//' '//cmult(imult)
        if(itu.eq.1 .and. ir.eq.0) msg='TU; '//trim(call_1)//' '//trim(call_2)//     &
             ' '//crpt//' '//cmult(imult)
        if(itu.eq.0 .and. ir.eq.1) msg=trim(call_1)//' '//trim(call_2)//             &
             ' R '//crpt//' '//cmult(imult)
        if(itu.eq.1 .and. ir.eq.1) msg='TU; '//trim(call_1)//' '//trim(call_2)//     &
             ' R '//crpt//' '//cmult(imult)
     else if(nserial.ge.1 .and. nserial.le.7999) then
        write(cserial,'(i4.4)') nserial
        if(itu.eq.0 .and. ir.eq.0) msg=trim(call_1)//' '//trim(call_2)//             &
             ' '//crpt//' '//cserial
        if(itu.eq.1 .and. ir.eq.0) msg='TU; '//trim(call_1)//' '//trim(call_2)//     &
             ' '//crpt//' '//cserial
        if(itu.eq.0 .and. ir.eq.1) msg=trim(call_1)//' '//trim(call_2)//             &
             ' R '//crpt//' '//cserial
        if(itu.eq.1 .and. ir.eq.1) msg='TU; '//trim(call_1)//' '//trim(call_2)//     &
             ' R '//crpt//' '//cserial
     endif
  else if(i3.eq.4) then
     read(c77,1050) n12,n58,iflip,nrpt,icq
1050 format(b12,b58,b1,b2,b1)
     do i=11,1,-1
        j=mod(n58,38)+1
        c11(i:i)=c(j:j)
        n58=n58/38
     enddo
     call hash12(n12,call_3)
     if(iflip.eq.0) then       ! 12 bit hash for TO call
        call_1=call_3
        call_2=adjustl(c11)//'          '
        call add_call_to_recent_calls(call_2)
        if(nrx.eq.1 .and.                        &
           dxcall13_set .and. mycall13_set .and. &
           call_2.eq.dxcall13 .and.              &
           n12.eq.hashmy12 ) call_1='<'//trim(mycall13)//'>'
        if(nrx.eq.1 .and.                        &
           mycall13_set .and.                    &
           index(call_1,'<...>').gt.0 .and.      &
           n12.eq.hashmy12 ) call_1='<'//trim(mycall13)//'>'
     else                      ! 12 bit hash for DE call
        call_1=adjustl(c11)
        call_2=call_3
        call add_call_to_recent_calls(call_1)
        if(nrx.eq.0 .and.                        &
           mycall13_set .and. &
           n12.eq.hashmy12) call_2='<'//trim(mycall13)//'>'
     endif
     if(icq.eq.0) then
        if(nrpt.eq.0) msg=trim(call_1)//' '//trim(call_2)
        if(nrpt.eq.1) msg=trim(call_1)//' '//trim(call_2)//' RRR'
        if(nrpt.eq.2) msg=trim(call_1)//' '//trim(call_2)//' RR73'
        if(nrpt.eq.3) msg=trim(call_1)//' '//trim(call_2)//' 73'
     else
        msg='CQ '//trim(call_2)
     endif
     msgcall=trim(call_2)
  endif
  if(msg(1:4).eq.'CQ <') unpk77_success=.false.

  return
end subroutine unpack77

subroutine pack28(c13,n28)

! Pack a special token, a 22-bit hash code, or a valid base call into a 28-bit
! integer.

  parameter (NTOKENS=2063592,MAX22=4194304)
  logical is_digit,is_letter
  character*13 c13
  character*6 callsign
  character*1 c
  character*4 c4
  character*37 a1
  character*36 a2
  character*10 a3
  character*27 a4
  data a1/' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'/
  data a2/'0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'/
  data a3/'0123456789'/
  data a4/' ABCDEFGHIJKLMNOPQRSTUVWXYZ'/

  is_digit(c)=c.ge.'0' .and. c.le.'9'
  is_letter(c)=c.ge.'A' .and. c.le.'Z'

  n28=-1
! Work-around for Swaziland prefix:
  if(c13(1:4).eq.'3DA0') callsign='3D0'//c13(5:7)
! Work-around for Guinea prefixes:
  if(c13(1:2).eq.'3X' .and. c13(3:3).ge.'A' .and.          &
       c13(3:3).le.'Z') callsign='Q'//c13(3:6)

! Check for special tokens first
  if(c13(1:3).eq.'DE ') then
     n28=0
     go to 900
  endif

  if(c13(1:4).eq.'QRZ ') then
     n28=1
     go to 900
  endif

  if(c13(1:3).eq.'CQ ') then
     n28=2
     go to 900
  endif

  if(c13(1:3).eq.'CQ_') then
     n=len(trim(c13))
     if(n.ge.4 .and. n.le.7) then
        nlet=0
        nnum=0
        do i=4,n
           c=c13(i:i)
           if(c.ge.'A' .and. c.le.'Z') nlet=nlet+1
           if(c.ge.'0' .and. c.le.'9') nnum=nnum+1
        enddo
        if(nnum.eq.3 .and. nlet.eq.0) then
           read(c13(4:3+nnum),*) nqsy
           n28=3+nqsy
           go to 900
        endif
        if(nlet.ge.1 .and. nlet.le.4 .and. nnum.eq.0) then
           c4=c13(4:n)
           c4=adjustr(c4)
           m=0
           do i=1,4
              j=0
              c=c4(i:i)
              if(c.ge.'A' .and. c.le.'Z') j=ichar(c)-ichar('A')+1
              m=27*m + j
           enddo
           n28=3+1000+m
           go to 900
        endif
     endif
  endif

! Check for <...> callsign
  if(c13(1:1).eq.'<')then
     call save_hash_call(c13,n10,n12,n22)   !Save callsign in hash table
     i2=index(c13,'>')
     c13=c13(2:i2-1)
     n22=ihashcall(c13,22)
     n28=NTOKENS + n22
     go to 900
  endif

! Check for standard callsign
  iarea=-1
  n=len(trim(c13))
  do i=n,2,-1
     if(is_digit(c13(i:i))) exit
  enddo
  iarea=i                                   !Call-area digit
  npdig=0                                   !Digits before call area
  nplet=0                                   !Letters before call area
  do i=1,iarea-1
     if(is_digit(c13(i:i))) npdig=npdig+1
     if(is_letter(c13(i:i))) nplet=nplet+1
  enddo
  nslet=0
  do i=iarea+1,n
     if(is_letter(c13(i:i))) nslet=nslet+1
  enddo
  if(iarea.lt.2 .or. iarea.gt.3 .or. nplet.eq.0 .or.       &
       npdig.ge.iarea-1 .or. nslet.gt.3) then
! Treat this as a nonstandard callsign: compute its 22-bit hash
     call save_hash_call(c13,n10,n12,n22)   !Save callsign in hash table
     n22=ihashcall(c13,n22)
     n28=NTOKENS + n22
     go to 900
  endif

  n=len(trim(c13))
! This is a standard callsign
  call save_hash_call(c13,n10,n12,n22)   !Save callsign in hash table
  if(iarea.eq.2) callsign=' '//c13(1:5)
  if(iarea.eq.3) callsign=c13(1:6)
  i1=index(a1,callsign(1:1))-1
  i2=index(a2,callsign(2:2))-1
  i3=index(a3,callsign(3:3))-1
  i4=index(a4,callsign(4:4))-1
  i5=index(a4,callsign(5:5))-1
  i6=index(a4,callsign(6:6))-1
  n28=36*10*27*27*27*i1 + 10*27*27*27*i2 + 27*27*27*i3 + 27*27*i4 + &
       27*i5 + i6
  n28=n28 + NTOKENS + MAX22

900 n28=iand(n28,ishft(1,28)-1)
  return
end subroutine pack28


subroutine unpack28(n28_0,c13,success)

  parameter (NTOKENS=2063592,MAX22=4194304)
  logical success
  character*13 c13
  character*37 c1
  character*36 c2
  character*10 c3
  character*27 c4
  data c1/' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'/
  data c2/'0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'/
  data c3/'0123456789'/
  data c4/' ABCDEFGHIJKLMNOPQRSTUVWXYZ'/

  success=.true.
  n28=n28_0
  if(n28.lt.NTOKENS) then
! Special tokens DE, QRZ, CQ, CQ_nnn, CQ_aaaa
     if(n28.eq.0) c13='DE           '
     if(n28.eq.1) c13='QRZ          '
     if(n28.eq.2) c13='CQ           '
     if(n28.le.2) go to 900
     if(n28.le.1002) then
        write(c13,1002) n28-3
1002    format('CQ_',i3.3)
        go to 900
     endif
     if(n28.le.532443) then
        n=n28-1003
        n0=n
        i1=n/(27*27*27)
        n=n-27*27*27*i1
        i2=n/(27*27)
        n=n-27*27*i2
        i3=n/27
        i4=n-27*i3
        c13=c4(i1+1:i1+1)//c4(i2+1:i2+1)//c4(i3+1:i3+1)//c4(i4+1:i4+1)
        c13=adjustl(c13)
        c13='CQ_'//c13(1:10)
        go to 900
     endif
  endif
  n28=n28-NTOKENS
  if(n28.lt.MAX22) then
! This is a 22-bit hash of a callsign
     n22=n28
     call hash22(n22,c13)     !Retrieve callsign from hash table
     go to 900
  endif

! Standard callsign
  n=n28 - MAX22
  i1=n/(36*10*27*27*27)
  n=n-36*10*27*27*27*i1
  i2=n/(10*27*27*27)
  n=n-10*27*27*27*i2
  i3=n/(27*27*27)
  n=n-27*27*27*i3
  i4=n/(27*27)
  n=n-27*27*i4
  i5=n/27
  i6=n-27*i5
  c13=c1(i1+1:i1+1)//c2(i2+1:i2+1)//c3(i3+1:i3+1)//c4(i4+1:i4+1)//     &
       c4(i5+1:i5+1)//c4(i6+1:i6+1)
  c13=adjustl(c13)

900 i0=index(c13,' ')
  if(i0.ne.0 .and. i0.lt.len(trim(c13))) then
     c13='QU1RK'
     success=.false.
  endif
  return
end subroutine unpack28

subroutine split77(msg,nwords,nw,w)

! Convert msg to upper case; collapse multiple blanks; parse into words.

  character*37 msg
  character*13 w(19)
  character*1 c,c0
  character*6 bcall_1
  logical ok1
  integer nw(19)

  iz=len(trim(msg))
  j=0
  k=0
  n=0
  c0=' '
  w='             '
  do i=1,iz
     if(ichar(msg(i:i)).eq.0) msg(i:i)=' '
     c=msg(i:i)                                 !Single character
     if(c.eq.' ' .and. c0.eq.' ') cycle         !Skip leading/repeated blanks
     if(c.ne.' ' .and. c0.eq.' ') then
        k=k+1                                   !New word
        n=0
     endif
     j=j+1                                      !Index in msg
     n=n+1                                      !Index in word
     msg(j:j)=c
     if(c.ge.'a' .and. c.le.'z') msg(j:j)=char(ichar(c)-32)  !Force upper case
     if(n.le.13) w(k)(n:n)=c                    !Copy character c into word
     c0=c
  enddo
  iz=j                                          !Message length
  nwords=k                                      !Number of words in msg
  if(nwords.le.0) go to 900
  nw(k)=len(trim(w(k)))
  msg(iz+1:)='                                     '
  if(nwords.lt.3) go to 900
  call chkcall(w(3),bcall_1,ok1)
  if(ok1 .and. w(1)(1:3).eq.'CQ ') then
     w(1)='CQ_'//w(2)(1:10)             !Make "CQ " into "CQ_"
     w(2:12)=w(3:13)                    !Move all remeining words down by one
     nwords=nwords-1
  endif

900 return
end subroutine split77


subroutine pack77_01(nwords,w,i3,n3,c77)

! Pack a Type 0.1 message: DXpedition mode
! Example message:  "K1ABC RR73; W9XYZ <KH1/KH7Z> -11"   28 28 10 5

  character*13 w(19),c13
  character*77 c77
  character*6 bcall_1,bcall_2
  logical ok1,ok2

  if(nwords.ne.5) go to 900                !Must have 5 words
  if(trim(w(2)).ne.'RR73;') go to 900      !2nd word must be "RR73;"
  if(w(4)(1:1).ne.'<') go to 900           !4th word must have <...>
  if(index(w(4),'>').lt.1) go to 900
  n=-99
  read(w(5),*,err=1) n
1 if(n.eq.-99) go to 900                   !5th word must be a valid report
  n5=(n+30)/2
  if(n5.lt.0) n5=0
  if(n5.gt.31) n5=31
  call chkcall(w(1),bcall_1,ok1)
  if(.not.ok1) go to 900                   !1st word must be a valid basecall
  call chkcall(w(3),bcall_2,ok2)
  if(.not.ok2) go to 900                   !3rd word must be a valid basecall

! Type 0.1:  K1ABC RR73; W9XYZ <KH1/KH7Z> -11   28 28 10 5       71   DXpedition Mode
  i3=0
  n3=1
  call pack28(w(1),n28a)
  call pack28(w(3),n28b)
  call save_hash_call(w(4),n10,n12,n22)
  i2=index(w(4),'>')
  c13=w(4)(2:i2-1)
  n10=ihashcall(c13,10)
  write(c77,1010) n28a,n28b,n10,n5,n3,i3
1010 format(2b28.28,b10.10,b5.5,2b3.3)

900 return
end subroutine pack77_01


subroutine pack77_02(nwords,w,i3,n3,c77)

  character*13 w(19),c13
  character*77 c77
  character*6 bcall_1,grid6
  logical ok1,is_grid6

  is_grid6(grid6)=len(trim(grid6)).eq.6 .and.                        &
       grid6(1:1).ge.'A' .and. grid6(1:1).le.'R' .and.               &
       grid6(2:2).ge.'A' .and. grid6(2:2).le.'R' .and.               &
       grid6(3:3).ge.'0' .and. grid6(3:3).le.'9' .and.               &
       grid6(4:4).ge.'0' .and. grid6(4:4).le.'9' .and.               &
       grid6(5:5).ge.'A' .and. grid6(5:5).le.'X' .and.               &
       grid6(6:6).ge.'A' .and. grid6(6:6).le.'X'

  call chkcall(w(1),bcall_1,ok1)
  if(.not.ok1) return                            !bcall_1 must be a valid basecall
  if(nwords.lt.3 .or. nwords.gt.4) return        !nwords must be 3 or 4
  nx=-1
  if(nwords.ge.2) read(w(nwords-1),*,err=2) nx
2 if(nx.lt.520001 .or. nx.gt.594095) return      !Exchange between 520001 - 594095
  if(.not.is_grid6(w(nwords)(1:6))) return       !Last word must be a valid grid6

! Type 0.2:   PA3XYZ/P R 590003 IO91NP           28 1 1 3 12 25   70   EU VHF contest
  i3=0
  n3=2
  i=index(w(1)//' ','/P ')
  if(i.ge.4) then
     ip=1
     c13=w(1)(1:i-1)
  else
     ip=0
     c13=w(1)
  end if
  call pack28(c13,n28a)
  ir=0
  if(w(2)(1:2).eq.'R ') ir=1
  irpt=nx/10000 - 52
  iserial=mod(nx,10000)
  grid6=w(nwords)(1:6)
  j1=(ichar(grid6(1:1))-ichar('A'))*18*10*10*24*24
  j2=(ichar(grid6(2:2))-ichar('A'))*10*10*24*24
  j3=(ichar(grid6(3:3))-ichar('0'))*10*24*24
  j4=(ichar(grid6(4:4))-ichar('0'))*24*24
  j5=(ichar(grid6(5:5))-ichar('A'))*24
  j6=(ichar(grid6(6:6))-ichar('A'))
  igrid6=j1+j2+j3+j4+j5+j6
  write(c77,1010) n28a,ip,ir,irpt,iserial,igrid6,n3,i3
1010 format(b28.28,2b1,b3.3,b12.12,b25.25,b4.4,b3.3)

  return
end subroutine pack77_02


subroutine pack77_03(nwords,w,i3,n3,c77)
! Check 0.3 and 0.4 (ARRL Field Day exchange)

  parameter (NSEC=84)      !Number of ARRL Sections
  character*13 w(19)
  character*77 c77
  character*6 bcall_1,bcall_2
  character*3 csec(NSEC)
  logical ok1,ok2
  data csec/                                                         &
       "AB ","AK ","AL ","AR ","AZ ","BC ","CO ","CT ","DE ","EB ",  &
       "EMA","ENY","EPA","EWA","GA ","GTA","IA ","ID ","IL ","IN ",  &
       "KS ","KY ","LA ","LAX","MAR","MB ","MDC","ME ","MI ","MN ",  &
       "MO ","MS ","MT ","NC ","ND ","NE ","NFL","NH ","NL ","NLI",  &
       "NM ","NNJ","NNY","NT ","NTX","NV ","OH ","OK ","ONE","ONN",  &
       "ONS","OR ","ORG","PAC","PR ","QC ","RI ","SB ","SC ","SCV",  &
       "SD ","SDG","SF ","SFL","SJV","SK ","SNJ","STX","SV ","TN ",  &
       "UT ","VA ","VI ","VT ","WCF","WI ","WMA","WNY","WPA","WTX",  &
       "WV ","WWA","WY ","DX "/

  if(nwords.lt.4 .or. nwords.gt.5) return
  call chkcall(w(1),bcall_1,ok1)
  call chkcall(w(2),bcall_2,ok2)
  if(.not.ok1 .or. .not.ok2) return
  isec=-1
  do i=1,NSEC
     if(csec(i).eq.w(nwords)(1:3)) then
        isec=i
        exit
     endif
  enddo
  if(isec.eq.-1) return
  if(nwords.eq.5 .and. trim(w(3)).ne.'R') return

  ntx=-1
  j=len(trim(w(nwords-1)))-1
  read(w(nwords-1)(1:j),*,err=1) ntx                !Number of transmitters
1 if(ntx.lt.1 .or. ntx.gt.32) return
  nclass=ichar(w(nwords-1)(j+1:j+1))-ichar('A')

  m=len(trim(w(nwords)))                            !Length of section abbreviation
  if(m.lt.2 .or. m.gt.3) return

! 0.3   WA9XYZ KA1ABC R 16A EMA            28 28 1 4 3 7    71   ARRL Field Day
! 0.4   WA9XYZ KA1ABC R 32A EMA            28 28 1 4 3 7    71   ARRL Field Day

  i3=0
  n3=3                                 !Type 0.3 ARRL Field Day
  intx=ntx-1
  if(intx.ge.16) then
     n3=4                              !Type 0.4 ARRL Field Day
     intx=ntx-17
  endif
  call pack28(w(1),n28a)
  call pack28(w(2),n28b)
  ir=0
  if(w(3)(1:2).eq.'R ') ir=1
  write(c77,1010) n28a,n28b,ir,intx,nclass,isec,n3,i3
1010 format(2b28.28,b1,b4.4,b3.3,b7.7,2b3.3)

  return
end subroutine pack77_03

subroutine pack77_1(nwords,w,i3,n3,c77)
! Check Type 1 (Standard 77-bit message) and Type 2 (ditto, with a "/P" call)

  parameter (MAXGRID4=32400)
  character*13 w(19),c13
  character*77 c77
  character*6 bcall_1,bcall_2
  character*4 grid4
  character c1*1,c2*2
  logical is_grid4
  logical ok1,ok2
  is_grid4(grid4)=len(trim(grid4)).eq.4 .and.                        &
       grid4(1:1).ge.'A' .and. grid4(1:1).le.'R' .and.               &
       grid4(2:2).ge.'A' .and. grid4(2:2).le.'R' .and.               &
       grid4(3:3).ge.'0' .and. grid4(3:3).le.'9' .and.               &
       grid4(4:4).ge.'0' .and. grid4(4:4).le.'9'

  if(nwords.lt.2 .or. nwords.gt.4) return
  call chkcall(w(1),bcall_1,ok1)
  call chkcall(w(2),bcall_2,ok2)
  if(w(1)(1:3).eq.'DE ' .or. w(1)(1:3).eq.'CQ_' .or.  w(1)(1:3).eq.'CQ ' .or. &
       w(1)(1:4).eq.'QRZ ') ok1=.true.
  if(w(1)(1:1).eq.'<' .and. index(w(1),'>').ge.5) ok1=.true.
  if(w(2)(1:1).eq.'<' .and. index(w(2),'>').ge.5) ok2=.true.
  if(.not.ok1 .or. .not.ok2) return
  if(w(1)(1:1).eq.'<' .and. index(w(2),'/').gt.0) return
  if(w(2)(1:1).eq.'<' .and. index(w(1),'/').gt.0) return
  if(nwords.eq.2 .and. (.not.ok2 .or. index(w(2),'/').ge.2)) return
  if(nwords.eq.2) go to 10

  c1=w(nwords)(1:1)
  c2=w(nwords)(1:2)
  if(.not.is_grid4(w(nwords)(1:4)) .and. c1.ne.'+' .and. c1.ne.'-'              &
       .and. c2.ne.'R+' .and. c2.ne.'R-' .and. trim(w(nwords)).ne.'RRR' .and.   &
       trim(w(nwords)).ne.'RR73' .and. trim(w(nwords)).ne.'73') return
  if(c1.eq.'+' .or. c1.eq.'-') then
     ir=0
     read(w(nwords),*,err=900) irpt
     irpt=irpt+35
  else if(c2.eq.'R+' .or. c2.eq.'R-') then
     ir=1
     read(w(nwords)(2:),*) irpt
     irpt=irpt+35
  else if(trim(w(nwords)).eq.'RRR') then
     ir=0
     irpt=2
  else if(trim(w(nwords)).eq.'RR73') then
     ir=0
     irpt=3
  else if(trim(w(nwords)).eq.'73') then
     ir=0
     irpt=4
  endif

! 1     WA9XYZ/R KA1ABC/R R FN42           28 1 28 1 1 15   74   Standard msg
! 2     PA3XYZ/P GM4ABC/P R JO22           28 1 28 1 1 15   74   EU VHF contest

10 i1psuffix=index(w(1)//' ' ,'/P ')
  i2psuffix=index(w(2)//' ','/P ')
  if(nwords.eq.2 .or. nwords.eq.3 .or. (nwords.eq.4 .and.               &
       w(3)(1:2).eq.'R ')) then
     n3=0
     i3=1                          !Type 1: Standard message, possibly with "/R"
     if (i1psuffix.ge.4.or.i2psuffix.ge.4) i3=2 !Type 2, with "/P"
  endif
  c13=bcall_1
  if(c13(1:3).eq.'CQ_' .or. w(1)(1:1).eq.'<') c13=w(1)
  call pack28(c13,n28a)
  c13=bcall_2
  if(w(2)(1:1).eq.'<') c13=w(2)
  call pack28(c13,n28b)
  ipa=0
  ipb=0
  if(i1psuffix.ge.4.or.index(w(1)//' ','/R ').ge.4) ipa=1
  if(i2psuffix.ge.4.or.index(w(2)//' ','/R ').ge.4) ipb=1

  grid4=w(nwords)(1:4)
  if(is_grid4(grid4)) then
     ir=0
     if(w(3).eq.'R ') ir=1
     j1=(ichar(grid4(1:1))-ichar('A'))*18*10*10
     j2=(ichar(grid4(2:2))-ichar('A'))*10*10
     j3=(ichar(grid4(3:3))-ichar('0'))*10
     j4=(ichar(grid4(4:4))-ichar('0'))
     igrid4=j1+j2+j3+j4
  else
     igrid4=MAXGRID4 + irpt
  endif
  if(nwords.eq.2) then
     ir=0
     irpt=1
     igrid4=MAXGRID4+irpt
  endif
  write(c77,1000) n28a,ipa,n28b,ipb,ir,igrid4,i3
1000 format(2(b28.28,b1),b1,b15.15,b3.3)
  return

900 return
end subroutine pack77_1


subroutine pack77_3(nwords,w,i3,n3,c77)
! Check Type 2 (ARRL RTTY contest exchange)
!ARRL RTTY   - US/Can: rpt state/prov      R 579 MA
!     	     - DX:     rpt serial          R 559 0013

  parameter (NUSCAN=65)    !Number of US states and Canadian provinces/territories
  character*13 w(19)
  character*77 c77
  character*6 bcall_1,bcall_2
  character*3 cmult(NUSCAN),mult
  character crpt*3
  logical ok1,ok2
  data cmult/                                                        &
       "AL ","AK ","AZ ","AR ","CA ","CO ","CT ","DE ","FL ","GA ",  &
       "HI ","ID ","IL ","IN ","IA ","KS ","KY ","LA ","ME ","MD ",  &
       "MA ","MI ","MN ","MS ","MO ","MT ","NE ","NV ","NH ","NJ ",  &
       "NM ","NY ","NC ","ND ","OH ","OK ","OR ","PA ","RI ","SC ",  &
       "SD ","TN ","TX ","UT ","VT ","VA ","WA ","WV ","WI ","WY ",  &
       "NB ","NS ","QC ","ON ","MB ","SK ","AB ","BC ","NWT","NF ",  &
       "LB ","NU ","YT ","PEI","DC "/

  if(nwords.eq.4 .or. nwords.eq.5 .or. nwords.eq.6) then
     i1=1
     if(trim(w(1)).eq.'TU;') i1=2
     call chkcall(w(i1),bcall_1,ok1)
     call chkcall(w(i1+1),bcall_2,ok2)
     if(.not.ok1 .or. .not.ok2) go to 900
     crpt=w(nwords-1)(1:3)
     if(crpt(1:1).eq.'5' .and. crpt(2:2).ge.'2' .and. crpt(2:2).le.'9' .and.    &
          crpt(3:3).eq.'9') then
        nserial=0
        read(w(nwords),*,err=1) nserial
!1       i3=3
!        n3=0
     endif
1    mult='   '
     imult=-1
     do i=1,NUSCAN
        if(cmult(i).eq.w(nwords)) then
           imult=i
           mult=cmult(i)
           exit
        endif
     enddo
     nexch=0
     if(nserial.gt.0) nexch=nserial
     if(imult.gt.0) nexch=8000+imult
     if(mult.ne.'   ' .or. nserial.gt.0) then
        i3=3
        n3=0
        itu=0
        if(trim(w(1)).eq.'TU;') itu=1
        call pack28(w(1+itu),n28a)
        call pack28(w(2+itu),n28b)
        ir=0
        if(w(3+itu)(1:2).eq.'R ') ir=1
        read(w(3+itu+ir),*,err=900) irpt
        irpt=(irpt-509)/10 - 2
        if(irpt.lt.0) irpt=0
        if(irpt.gt.7) irpt=7
! 3     TU; W9XYZ K1ABC R 579 MA             1 28 28 1 3 13       74   ARRL RTTY contest
! 3     TU; W9XYZ G8ABC R 559 0013           1 28 28 1 3 13       74   ARRL RTTY (DX)
        write(c77,1010) itu,n28a,n28b,ir,irpt,nexch,i3
1010    format(b1,2b28.28,b1,b3.3,b13.13,b3.3)
     endif
  endif

900 return
end subroutine pack77_3

subroutine pack77_4(nwords,w,i3,n3,c77)
! Check Type 3 (One nonstandard call and one hashed call)

  integer*8 n58
  logical ok1,ok2
  character*13 w(19)
  character*77 c77
  character*13 call_1,call_2
  character*11 c11
  character*6 bcall_1,bcall_2
  character*38 c
  data c/' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/'/

  iflip=0
  i3=-1
  if(nwords.eq.2 .or. nwords.eq.3) then
     call_1=w(1)
     if(call_1(1:1).eq.'<') call_1=w(1)(2:len(trim(w(1)))-1)
     call_2=w(2)
     if(call_2(1:1).eq.'<') call_2=w(2)(2:len(trim(w(2)))-1)
     call chkcall(call_1,bcall_1,ok1)
     call chkcall(call_2,bcall_2,ok2)
     icq=0
     if(trim(w(1)).eq.'CQ' .or. (ok1.and.ok2)) then
        if(trim(w(1)).eq.'CQ' .and. len(trim(w(2))).le.4) go to 900
        i3=4
        n3=0
        if(trim(w(1)).eq.'CQ') icq=1
     endif

     if(icq.eq.1) then
        iflip=0
        n12=0
        c11=adjustr(call_2(1:11))
        call save_hash_call(w(2),n10,n12,n22)
     else if(w(1)(1:1).eq.'<') then
        iflip=0
        i3=4
        call save_hash_call(w(1),n10,n12,n22)
        c11=adjustr(call_2(1:11))
     else if(w(2)(1:1).eq.'<') then
        iflip=1
        i3=4
        call save_hash_call(w(2),n10,n12,n22)
        c11=adjustr(call_1(1:11))
     endif
     n58=0
     do i=1,11
        n58=n58*38 + index(c,c11(i:i)) - 1
     enddo
     nrpt=0
     if(trim(w(3)).eq.'RRR') nrpt=1
     if(trim(w(3)).eq.'RR73') nrpt=2
     if(trim(w(3)).eq.'73') nrpt=3
     if(icq.eq.1) then
        iflip=0
        nrpt=0
     endif
     write(c77,1010) n12,n58,iflip,nrpt,icq,i3
1010 format(b12.12,b58.58,b1,b2.2,b1,b3.3)
     do i=1,77
        if(c77(i:i).eq.'*') c77(i:i)='0'     !### Clean up any illegal chars ###
     enddo
  endif

900 return
end subroutine pack77_4

subroutine packtext77(c13,c71)

  character*13 c13,w
  character*71 c71
  character*42 c
  character*1 qa(10),qb(10)
  data c/' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?'/

  call mp_short_init
  qa=char(0)
  w=adjustr(c13)
  do i=1,13
     j=index(c,w(i:i))-1
     if(j.lt.0) j=0
     call mp_short_mult(qb,qa(2:10),9,42)     !qb(1:9)=42*qa(2:9)
     call mp_short_add(qa,qb(2:10),9,j)      !qa(1:9)=qb(2:9)+j
  enddo

  write(c71,1010) qa(2:10)
1010 format(b7.7,8b8.8)

  return
end subroutine packtext77

subroutine unpacktext77(c71,c13)

  integer*1   ia(10)
  character*1 qa(10),qb(10)
  character*13 c13
  character*71 c71
  character*42 c
  equivalence (qa,ia),(qb,ib)
  data c/' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?'/

  qa(1)=char(0)
  read(c71,1010) qa(2:10)
1010 format(b7.7,8b8.8)

  do i=13,1,-1
     call mp_short_div(qb,qa(2:10),9,42,ir)
     c13(i:i)=c(ir+1:ir+1)
     qa(2:10)=qb(1:9)
  enddo

  return
end subroutine unpacktext77

subroutine mp_short_ops(w,u)
  character*1 w(*),u(*)
  integer i,ireg,j,n,ir,iv,ii1,ii2
  character*1 creg(4)
  save ii1,ii2
  equivalence (ireg,creg)

  entry mp_short_init
  ireg=256*ichar('2')+ichar('1')
  do j=1,4
     if (creg(j).eq.'1') ii1=j
     if (creg(j).eq.'2') ii2=j
  enddo
  return

  entry mp_short_add(w,u,n,iv)
  ireg=256*iv
  do j=n,1,-1
     ireg=ichar(u(j))+ichar(creg(ii2))
     w(j+1)=creg(ii1)
  enddo
  w(1)=creg(ii2)
  return

  entry mp_short_mult(w,u,n,iv)
  ireg=0
  do j=n,1,-1
     ireg=ichar(u(j))*iv+ichar(creg(ii2))
     w(j+1)=creg(ii1)
  enddo
  w(1)=creg(ii2)
  return

  entry mp_short_div(w,u,n,iv,ir)
  ir=0
  do j=1,n
     i=256*ir+ichar(u(j))
     w(j)=char(i/iv)
     ir=mod(i,iv)
  enddo
  return

  return
end subroutine mp_short_ops

subroutine add_call_to_recent_calls(callsign)

  character*13 callsign
  logical ladd

! only add if the callsign is not already on the list
  ladd=.true.
  do i=1,MAXRECENT-1 ! if callsign is at the end of the list add it again
     if(recent_calls(i).eq.callsign) ladd=.false.
  enddo

  if(ladd) then
     do i=MAXRECENT,2,-1
        recent_calls(i)=recent_calls(i-1)
     enddo
     recent_calls(1)=callsign
  endif

! Make sure that callsign is hashed
  call save_hash_call(callsign,n10,n12,n22)

  return
end subroutine add_call_to_recent_calls

end module packjt77
