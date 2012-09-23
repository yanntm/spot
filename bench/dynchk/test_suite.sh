#! /bin/sh 

. ./defs

FILE=/tmp/mytest

OUTPUT=LATEX

# This script process results for all algorithms 
resfile=$(mktemp /tmp/dynchk.XXXXXX)
resfilePO=$(mktemp /tmp/dynchkPO.XXXXXX)
ranking=$(mktemp /tmp/ranking.XXXXXX)
rankingPO=$(mktemp /tmp/rankingPO.XXXXXX)

# Walk all test suite 
for file in $(ls models/*test_suite); do 
  echo processing $file
  MODEL=$(basename $file .test_suite)

  # Process with classical kripke 
  ./dynchk -r7 -Pmodels/"$MODEL".tgba $file > "$resfile""-$MODEL"
  cat "$resfile""-$MODEL" >> "$resfile"

  # Process with PO kripke
  ./dynchk -r7 -Pmodels/"$MODEL"R.tgba $file > "$resfilePO""-$MODEL"
  cat "$resfilePO""-$MODEL"  >> $resfilePO
done;

echo 
echo

# Now  process For LaTeX
if [ "$OUTPUT" = "LATEX" ]; then

    # Fetch BA and TGBA algorithms used 
    # under the hypothesis that algorithms used with Kripke 
    # and Partial Order Kripke are the sames 
    BA_ALGO=$(grep -v "#"  $resfilePO | grep ",BA," | sed s/,BA.*//g | sed s/,.*//g | awk '!x[$0]++')
    TGBA_ALGO=$(grep -v "#"  $resfilePO | grep ",TGBA," | sed s/,BA.*//g | sed s/,.*//g | awk '!x[$0]++')


   #
   # Compute the ranking for models without Partial Order Reduction
   #
   for file in $(ls models/*test_suite); do 
       MODEL=$(basename $file .test_suite)
       for algo in $(echo $BA_ALGO); do
	   grep  ",BA,"  "$resfile-""$MODEL" | grep  "$algo," > "$resfile-""$MODEL"-tmp
	   N=$(cat "$resfile-""$MODEL"-tmp | wc -l)
	   TIME=$(cat "$resfile-""$MODEL"-tmp | awk -F"," '{print $4}' | tr '\n' '+')
	   MTIME=$(echo "scale=2; ($TIME 0)/$N" | bc)
	   echo $MTIME $algo   >> "$resfile-""$MODEL"-tmp2
	done;
       cat "$resfile-""$MODEL"-tmp2 | sort  | sed "s/.* \(.*\)/\1/g" | nl > "$ranking"-"$MODEL".txt



       for algo in $(echo $TGBA_ALGO); do
	   grep  ",TGBA,"  "$resfile-""$MODEL" | grep  "$algo," > "$resfile-""$MODEL"-TGBA-tmp
	   N=$(cat "$resfile-""$MODEL"-TGBA-tmp | wc -l)
	   TIME=$(cat "$resfile-""$MODEL"-TGBA-tmp | awk -F"," '{print $4}' | tr '\n' '+')
	   MTIME=$(echo "scale=2; ($TIME 0)/$N" | bc)
	   echo $MTIME $algo   >> "$resfile-""$MODEL"-TGBA-tmp2
       done;
       cat "$resfile-""$MODEL"-TGBA-tmp2 | sort  | sed "s/.* \(.*\)/\1/g" | nl > "$ranking"-"$MODEL"-TGBA.txt
   done

   for algo in $(echo $BA_ALGO); do
       ranks=
       for file in $(ls models/*test_suite); do
	   MODEL=$(basename $file .test_suite)
	   rks=$(grep -e "$algo$" "$ranking"-"$MODEL".txt)
	   rks=$(echo $rks | sed  "s/ $algo//g")
	   ranks="$ranks $rks"
       done
       echo "$algo $ranks" >> $ranking
   done

   for algo in $(echo $TGBA_ALGO); do
       ranks=
       for file in $(ls models/*test_suite); do
	   MODEL=$(basename $file .test_suite)
	   rks=$(grep -e "$algo$" "$ranking"-"$MODEL"-TGBA.txt)
	   rks=$(echo $rks | sed  "s/ $algo//g")
	   ranks="$ranks $rks"
       done
       echo "$algo $ranks" >> $ranking-TGBA
   done

   #
   # Compute the ranking for models with Partial Order Reduction
   #
   for file in $(ls models/*test_suite); do 
       MODEL=$(basename $file .test_suite)
       for algo in $(echo $BA_ALGO); do
	   grep  ",BA,"  "$resfilePO-""$MODEL" | grep  "$algo," > "$resfilePO-""$MODEL"-tmp
	   N=$(cat "$resfilePO-""$MODEL"-tmp | wc -l)
	   TIME=$(cat "$resfilePO-""$MODEL"-tmp | awk -F"," '{print $4}' | tr '\n' '+')
	   MTIME=$(echo "scale=2; ($TIME 0)/$N" | bc)
	   echo $MTIME $algo   >> "$resfilePO-""$MODEL"-tmp2
	done;
       cat "$resfilePO-""$MODEL"-tmp2 | sort  | sed "s/.* \(.*\)/\1/g" | nl > "$rankingPO"-"$MODEL".txt
       
       for algo in $(echo $TGBA_ALGO); do
	   grep  ",TGBA,"  "$resfilePO-""$MODEL" | grep  "$algo," > "$resfilePO-""$MODEL"-TGBA-tmp
	   N=$(cat "$resfilePO-""$MODEL"-TGBA-tmp | wc -l)
	   TIME=$(cat "$resfilePO-""$MODEL"-TGBA-tmp | awk -F"," '{print $4}' | tr '\n' '+')
	   MTIME=$(echo "scale=2; ($TIME 0)/$N" | bc)
	   echo $MTIME $algo   >> "$resfilePO-""$MODEL"-TGBA-tmp2
       done;
       cat "$resfilePO-""$MODEL"-TGBA-tmp2 | sort  | sed "s/.* \(.*\)/\1/g" | nl > "$rankingPO"-"$MODEL"-TGBA.txt
   done
	   
   for algo in $(echo $BA_ALGO); do
       ranks=
       for file in $(ls models/*test_suite); do
	   MODEL=$(basename $file .test_suite)
	   rks=$(grep -e "$algo$" "$rankingPO"-"$MODEL".txt)
	   rks=$(echo $rks | sed  "s/ $algo//g")
	   ranks="$ranks $rks"
       done
       echo "$algo $ranks" >> $rankingPO
   done

   for algo in $(echo $TGBA_ALGO); do
       ranks=
       for file in $(ls models/*test_suite); do
	   MODEL=$(basename $file .test_suite)
	   rks=$(grep -e "$algo$" "$rankingPO"-"$MODEL"-TGBA.txt)
	   rks=$(echo $rks | sed  "s/ $algo//g")
	   ranks="$ranks $rks"
       done
       echo "$algo $ranks" >> $rankingPO-TGBA
   done


    #
    # Processing BA
    #
    resfileBAPO=$(mktemp /tmp/dynchk-BAPO.XXXXXX)
    resfileBA=$(mktemp /tmp/dynchk-BA.XXXXXX) 

    # Prepare output
cat <<EOF 
{ 
\renewcommand{\arraystretch}{1.5}
\begin{table}
  \centering
 {\scriptsize
   \begin{tabular}{|c|c|c|c|c|c|c|c|c|c|c|c|c|c|}
     \hline
     \multicolumn{2}{|c|}{ } & \multicolumn{6}{c|}{\textbf{standard Kripke}} &
     \multicolumn{6}{c|}{\textbf{Kripke with P.O.}} \\\\
     type& algo. & \textasciitilde states & \textasciitilde trans. & \textasciitilde time & rk min & av rk & rk max & \textasciitilde states & \textasciitilde trans. & \textasciitilde time & rk min & av rk  & rk max \\\\
     \hline
     \hline
\multirow{`echo $BA_ALGO | wc -w`}{*}{\begin{sideways}{BA}\end{sideways}}
EOF
    for algo in $(echo $BA_ALGO); do 
	# Process without PO
	grep  ",BA,"  $resfile | grep  "$algo," > $resfileBA
	N=$(cat $resfileBA | wc -l)
	TIME=$(cat $resfileBA | awk -F"," '{print $4}' | tr '\n' '+')
	MTIME=$(echo "scale=2; ($TIME 0)/$N" | bc)
	STATES=$(cat $resfileBA | awk -F"," '{print $5}' | tr '\n' '+')
	MSTATES=$(echo "scale=2; ($STATES 0)/$N" | bc)
	TRANS=$(cat $resfileBA | awk -F"," '{print $6}' | tr '\n' '+')
	MTRANS=$(echo "scale=2; ($TRANS 0)/$N" | bc)
	minRk=$(grep "$algo " $ranking | sed 's/[^ ]* //' | tr ' ' '\n'| sort -u  | tr '\n' ' ' | awk '{print $1}')
	maxRk=$(grep "$algo " $ranking | sed 's/[^ ]* //' | tr ' ' '\n'| sort -u  -r | tr '\n' ' ' | awk '{print $1}') 

	nBenchs=$(grep "$algo " $ranking | sed 's/[^ ]* //' | sed 's/ *//' | tr ' ' '\n' | wc -l )
	avRk=$(grep "$algo " $ranking | sed 's/[^ ]* //' | sed 's/ *//' | tr ' ' '+')
	avRk=$(echo "scale=2; ($avRk)/$nBenchs" | bc)

	# Process with PO
	grep  ",BA,"  $resfilePO | grep  "$algo," > $resfileBAPO
	N=$(cat $resfileBAPO | wc -l)
	TIMEPO=$(cat $resfileBAPO | awk -F"," '{print $4}' | tr '\n' '+')
	MTIMEPO=$(echo "scale=2; ($TIMEPO 0)/$N" | bc)
	STATESPO=$(cat $resfileBAPO | awk -F"," '{print $5}' | tr '\n' '+')
	MSTATESPO=$(echo "scale=2; ($STATESPO 0)/$N" | bc)
	TRANSPO=$(cat $resfileBAPO | awk -F"," '{print $6}' | tr '\n' '+')
	MTRANSPO=$(echo "scale=2; ($TRANSPO 0)/$N" | bc)
	minRkPO=$(grep "$algo " $rankingPO | sed 's/[^ ]* //' | tr ' ' '\n'| sort -u  | tr '\n' ' ' | awk '{print $1}')
	maxRkPO=$(grep "$algo " $rankingPO | sed 's/[^ ]* //' | tr ' ' '\n'| sort -u  -r | tr '\n' ' ' | awk '{print $1}') 

	nBenchsPO=$(grep "$algo " $rankingPO | sed 's/[^ ]* //' | sed 's/ *//' | tr ' ' '\n' | wc -l )
	avRkPO=$(grep "$algo " $rankingPO | sed 's/[^ ]* //' | sed 's/ *//' | tr ' ' '+')
	avRkPO=$(echo "scale=2; ($avRkPO)/$nBenchs" | bc)


	# Prepare output
	nAlgo=$(echo $algo | tr "_" "-")
	echo "&" $nAlgo " & "  $MSTATES " & " $MTRANS " & "  $MTIME " & "  $minRk " & " $avRk " & " $maxRk " & "  $MSTATESPO " & "  $MTRANSPO " & " $MTIMEPO " & " $minRkPO " & " $avRkPO  " &  " $maxRkPO '\\''\\' 
    done;
    rm $resfileBAPO $resfileBA

    echo

    #
    # Processing TGBA
    #
cat <<EOF
\hline 
\hline
\multirow{`echo $TGBA_ALGO | wc -w`}{*}{\begin{sideways}{TGBA}\end{sideways}}
EOF
    resfileTGBAPO=$(mktemp /tmp/dynchk-TGBAPO.XXXXXX)
    resfileTGBA=$(mktemp /tmp/dynchk-TGBA.XXXXXX)
    for algo in $(echo $TGBA_ALGO); do 
	# Process without PO
	grep  ",TGBA,"  $resfile | grep  "$algo," > $resfileTGBA
	N=$(cat $resfileTGBA | wc -l)
	TIME=$(cat $resfileTGBA | awk -F"," '{print $4}' | tr '\n' '+')
	MTIME=$(echo "scale=2; ($TIME 0)/$N" | bc)
	STATES=$(cat $resfileTGBA | awk -F"," '{print $5}' | tr '\n' '+')
	MSTATES=$(echo "scale=2; ($STATES 0)/$N" | bc)
	TRANS=$(cat $resfileTGBA | awk -F"," '{print $6}' | tr '\n' '+')
	MTRANS=$(echo "scale=2; ($TRANS 0)/$N" | bc)
	minRk=$(grep "$algo " $ranking-TGBA | sed 's/[^ ]* //' | tr ' ' '\n'| sort -u  | tr '\n' ' ' | awk '{print $1}')
	maxRk=$(grep "$algo " $ranking-TGBA | sed 's/[^ ]* //' | tr ' ' '\n'| sort -u  -r | tr '\n' ' ' | awk '{print $1}') 

	nBenchs=$(grep "$algo " $ranking-TGBA | sed 's/[^ ]* //' | sed 's/ *//' | tr ' ' '\n' | wc -l )
	avRk=$(grep "$algo " $ranking-TGBA | sed 's/[^ ]* //' | sed 's/ *//' | tr ' ' '+')
	avRk=$(echo "scale=2; ($avRk)/$nBenchs" | bc)

	# Process with PO
	grep  ",TGBA,"  $resfilePO | grep  "$algo," > $resfileTGBAPO
	N=$(cat $resfileTGBAPO | wc -l)
	TIMEPO=$(cat $resfileTGBAPO | awk -F"," '{print $4}' | tr '\n' '+')
	MTIMEPO=$(echo "scale=2; ($TIMEPO 0)/$N" | bc)
	STATESPO=$(cat $resfileTGBAPO | awk -F"," '{print $5}' | tr '\n' '+')
	MSTATESPO=$(echo "scale=2; ($STATESPO 0)/$N" | bc)
	TRANSPO=$(cat $resfileTGBAPO | awk -F"," '{print $6}' | tr '\n' '+')
	MTRANSPO=$(echo "scale=2; ($TRANSPO 0)/$N" | bc)
	minRkPO=$(grep "$algo " $rankingPO-TGBA | sed 's/[^ ]* //' | tr ' ' '\n'| sort -u  | tr '\n' ' ' | awk '{print $1}')
	maxRkPO=$(grep "$algo " $rankingPO-TGBA | sed 's/[^ ]* //' | tr ' ' '\n'| sort -u  -r | tr '\n' ' ' | awk '{print $1}') 

	nBenchsPO=$(grep "$algo " $rankingPO-TGBA | sed 's/[^ ]* //' | sed 's/ *//' | tr ' ' '\n' | wc -l )
	avRkPO=$(grep "$algo " $rankingPO-TGBA | sed 's/[^ ]* //' | sed 's/ *//' | tr ' ' '+')
	avRkPO=$(echo "scale=2; ($avRkPO)/$nBenchs" | bc)

	nAlgo=$(echo $algo | tr "_" "-")
	echo "&" $nAlgo " & "  $MSTATES " & " $MTRANS " & "  $MTIME " & "  $minRk " & " $avRk " & " $maxRk " & "  $MSTATESPO " & "  $MTRANSPO " & " $MTIMEPO " & " $minRkPO " & " $avRkPO  " &  " $maxRkPO '\\''\\' 
    done;

   # remove temporary files 
    rm $resfileTGBAPO $resfileTGBA

cat <<EOF
     \hline
   \end{tabular}
 }
 \caption{Evaluating performances of dynamic algorithms over BA and TGBA 
       \label{tab:bench_explicit}}
\end{table}
}
EOF


fi;



# clean up
rm -f $resfile* $resfilePO* $ranking* $rankingPO*











# Convert classic spot LTL file into spin
# while read line ; do  line="$line"; test=$(../../../src/ltltest/tostring -spin-syntax "$line"); echo $test ; done < eeaean1.test_suite


# # create the result directory for output
# RESDIR=$(pwd .)/results
# rm -Rf $RESDIR
# mkdir $RESDIR

# # Fix other variables 
# SPOTRES=$RESDIR/spot.txt
# SPINRES=$RESDIR/spin.txt


# for file in $(ls models/*test_suite); do 
#   echo processing $file
#   MODEL=$(basename $file .test_suite)

#   # Compute Spin results 
#   cd models
#   spin -f '<> noLeader V <> ! oneLeads' -a $MODEL.pml 
#   gcc -DCHECK pan.c
#   ./a.out >> $SPINRES
#   echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" >> $SPINRES
#   cd ..

#   while read algo; do 
#        # Compute Spot results 
#       "$LTL2TGBA" -lS -T -e$algo  -Pmodels/"$MODEL"R.tgba '<> noLeader V <> ! oneLeads' >> $SPOTRES--$algo
#       echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" >> $SPOTRES
#   done < algorithms-dyn;
# done;
# exit 1;




# # Compute Spin results 
# cd models
# spin -f '<> noLeader V <> ! oneLeads' -a eeaean1.pml 
# gcc -DCHECK pan.c
# ./a.out >> $SPINRES
# echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" >> $SPINRES
# cd ..


# # Compute Spot results 
# "$LTL2TGBA" -lS -T -eSE05_dyn  -Pmodels/eeaean1R.tgba '<> noLeader V <> ! oneLeads' >> $SPOTRES
# echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" >> $SPOTRES

# # Process Spot results 




