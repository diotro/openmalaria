param(
[string]$dir,
[System.Uri]$src
)

$install_path = $dir+"\"
$gsl_lib = $install_path+'gsl.lib.zip'
$gsl_dl = $src.AbsoluteUri

if((Test-Path $gsl_lib) -eq $false) {
    echo "Downloading compiled gsl library"
    echo $gsl_dl' -> '$gsl_lib
    (new-object System.Net.WebClient).DownloadFile($gsl_dl,$gsl_lib)
}

$lib = $install_path+"lib\"
echo $lib
$shell = new-object -com shell.application
$zip = $shell.NameSpace($gsl_lib)
echo $zip
echo $zip.items()
foreach($item in $zip.items())
{
  echo $item
  $shell.NameSpace($lib).copyhere($item)
}
