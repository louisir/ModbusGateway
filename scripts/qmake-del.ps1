param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $Paths
)

foreach ($path in $Paths) {
    Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
}

exit 0
