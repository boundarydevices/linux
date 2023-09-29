#!/usr/bin/awk -f
FNR==NR {
	x=index($0," ");
	if (x<=0) next;
	text=substr($0,x+1);
	if (!(text in a)) {
		a[text]=$1;
		cnt[text]=1;
	} else {
		cnt[text]=cnt[text]+1;
	}
	next;
};
{
	x=index($0," ");
	if (x<=0) next;
	text=substr($0,x+1);
	if (index(text, "Merge") > 0)
		next;
	if (!(text in a)) {
		print "git cherry-pick", $1, "   #"text
	} else {
		if (!(text in cntb)) {
			cntb[text]=1;
		} else {
			cntb[text]=cntb[text]+1;
			if (cntb[text] > cnt[text])
				print "git cherry-pick", $1, "   #dup!! "text
		}
	}
	next;
}
