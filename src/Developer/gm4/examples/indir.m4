define(`%%$$##', `>>>$0<<< cnt $#')

# indir(`%%$$##', nonsens, nonsens)
indir(`%%$$##', nonsens, nonsens)

# indir(`indir', `%%$$##', nonsens)
indir(`indir', `%%$$##', nonsens)

# indir(`indir', `indir', `indir', `indir', `%%$$##')
indir(`indir', `indir', `indir', `indir', `%%$$##')
