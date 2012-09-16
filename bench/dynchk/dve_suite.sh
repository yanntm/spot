#! /bin/sh 
#
# This script test all models/MODEL.dve_suite to perform 
# emptiness checks defined in dynchk.
# The output is a LaTeX one which analyses the whole 
# set of results.
. ./defs

FILE=/tmp/mytest

OUTPUT=LATEX

# This script process results for all algorithms 
resfile=$(mktemp /tmp/dynchk.XXXXXX)
ranking=$(mktemp /tmp/ranking.XXXXXX)

# Walk all test suite 
for file in $(ls models/*dve_suite); do 
  echo processing $file
  MODEL=$(basename $file .dve_suite)

  # Processing for the dve file 
  ./dynchk -r7 -Bmodels/"$MODEL" $file > "$resfile""-$MODEL"
  cat "$resfile""-$MODEL" >> "$resfile"

done;
echo 
echo

# Now  process For LaTeX
if [ "$OUTPUT" = "LATEX" ]; then

    # Fetch BA and TGBA algorithms used 
    BA_ALGO=$(grep -v "#"  $resfile | grep ",BA," | sed s/,BA.*//g | sed s/,.*//g | awk '!x[$0]++')
    TGBA_ALGO=$(grep -v "#"  $resfile | grep ",TGBA," | sed s/,BA.*//g | sed s/,.*//g | awk '!x[$0]++')


   #
   # Compute the ranking for dve model
   #
   for file in $(ls models/*dve_suite); do 
       MODEL=$(basename $file .dve_suite)
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
       for file in $(ls models/*dve_suite); do
	   MODEL=$(basename $file .dve_suite)
	   rks=$(grep -e "$algo$" "$ranking"-"$MODEL".txt)
	   rks=$(echo $rks | sed  "s/ $algo//g")
	   ranks="$ranks $rks"
       done
       echo "$algo $ranks" >> $ranking
   done

   for algo in $(echo $TGBA_ALGO); do
       ranks=
       for file in $(ls models/*dve_suite); do
	   MODEL=$(basename $file .dve_suite)
	   rks=$(grep -e "$algo$" "$ranking"-"$MODEL"-TGBA.txt)
	   rks=$(echo $rks | sed  "s/ $algo//g")
	   ranks="$ranks $rks"
       done
       echo "$algo $ranks" >> $ranking-TGBA
   done

    #
    # Processing BA
    #
    resfileBA=$(mktemp /tmp/dynchk-BA.XXXXXX) 

    # Prepare output
cat <<EOF 
{ 
\renewcommand{\arraystretch}{1.5}
\begin{table}
  \centering
 {\scriptsize
   \begin{tabular}{|c|c|c|c|c|c|c|c|}
     \hline
     \multicolumn{2}{|c|}{ } & \multicolumn{6}{c|}{\textbf{Dve Models}}  \\\\
     type& algo. & \textasciitilde states & \textasciitilde trans. & \textasciitilde time & rk min & av rk & rk max \\\\
     \hline
     \hline
\multirow{`echo $BA_ALGO | wc -w`}{*}{\begin{sideways}{BA}\end{sideways}}
EOF
    for algo in $(echo $BA_ALGO); do 
	# Process model
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


	# Prepare output
	nAlgo=$(echo $algo | tr "_" "-")
	echo "&" $nAlgo " & "  $MSTATES " & " $MTRANS " & "  $MTIME " & "  $minRk " & " $avRk " & " $maxRk  '\\''\\' 
    done;
    rm  $resfileBA

    echo

    #
    # Processing TGBA
    #
cat <<EOF
\hline 
\hline
\multirow{`echo $TGBA_ALGO | wc -w`}{*}{\begin{sideways}{TGBA}\end{sideways}}
EOF
    resfileTGBA=$(mktemp /tmp/dynchk-TGBA.XXXXXX)
    for algo in $(echo $TGBA_ALGO); do 
	# Process model
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


	nAlgo=$(echo $algo | tr "_" "-")
	echo "&" $nAlgo " & "  $MSTATES " & " $MTRANS " & "  $MTIME " & "  $minRk " & " $avRk " & " $maxRk  '\\''\\' 
    done;

   # remove temporary files 
    rm $resfileTGBA

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
rm -f $resfile*  $ranking*
