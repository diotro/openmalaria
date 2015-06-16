param(
[string]$dir,
[System.Uri]$src
)

$install_path = $dir+"\"
$zlib_lib = $install_path+'zlib.lib.zip'
$zlib_dl = $src.AbsoluteUri

if((Test-Path $zlib_lib) -eq $false) {
    echo "Downloading compiled zlib library"
    echo $zlib_dl' -> '$zlib_lib
    (new-object System.Net.WebClient).DownloadFile($zlib_dl,$zlib_lib)
}

$lib = $install_path+"lib\"
echo $lib
$shell = new-object -com shell.application
$zip = $shell.NameSpace($zlib_lib)
echo $zip
echo $zip.items()
foreach($item in $zip.items())
{  
  echo $item
  $shell.NameSpace($lib).copyhere($item)
}
