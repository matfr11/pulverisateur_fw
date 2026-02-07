# 1. Récupération de la branche
$branch = git rev-parse --abbrev-ref HEAD
Write-Host "Branche actuelle : $branch" -ForegroundColor Cyan

# 2. Confirmation
$choix = Read-Host "Voulez-vous pousser les modifications sur '$branch' ? (O/N)"
if ($choix -ne "O") { 
    Write-Host "Opération annulée." -ForegroundColor Yellow
    exit 
}

# 3. Message de commit
$msg = Read-Host "Message du commit"
if (-not $msg) { 
    Write-Host "Erreur : Le message de commit est obligatoire !" -ForegroundColor Red
    exit 
}

# 4. Exécution des commandes Git
Write-Host "Ajout des fichiers..." -ForegroundColor Gray
git add .

Write-Host "Création du commit..." -ForegroundColor Gray
git commit -m "$msg"

Write-Host "Envoi vers le serveur (Push)..." -ForegroundColor Yellow
# C'est cette ligne qui envoie tout sur Git
git push origin $branch

# 5. Résultat
if ($LASTEXITCODE -eq 0) {
    Write-Host "Sauvegarde OK ! Tout est en ligne." -ForegroundColor Green
} else {
    Write-Host "Erreur lors du Push ! Vérifiez votre connexion ou les conflits." -ForegroundColor Red
}