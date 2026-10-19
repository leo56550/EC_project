# feederd

Démon C qui contrôle le distributeur. Il reçoit des demandes de repas sur un
socket Unix et applique les règles : **2 repas maximum par jour**, **4 h
minimum entre deux repas**. Le compteur est sauvegardé sur disque, donc un
redémarrage ne le remet pas à zéro.

## Compiler et lancer (PC, moteur simulé)

```bash
make
./feederd -i 1        # intervalle réduit à 1 min pour tester
```

Options : `-s` socket (`/tmp/feeder.sock`), `-f` fichier d'état
(`feederd.state`), `-m` repas max (2), `-i` intervalle en minutes (240),
`-g` grammes par repas (50).

## Tester (dans un second terminal)

```bash
echo STATUS       | nc -U /tmp/feeder.sock   # meals=0/2 ... allowed=yes
echo REQUEST_MEAL | nc -U /tmp/feeder.sock   # OK 50
echo REQUEST_MEAL | nc -U /tmp/feeder.sock   # REFUSED trop tot, attendre 1 min
# ... 1 minute plus tard
echo REQUEST_MEAL | nc -U /tmp/feeder.sock   # OK 50
echo REQUEST_MEAL | nc -U /tmp/feeder.sock   # REFUSED max 2 repas/jour atteint
```

Remettre le compteur à zéro : arrêter feederd et `rm feederd.state`.

## Protocole

Une commande par connexion, terminée par `\n`.

| Commande       | Réponse |
|----------------|---------|
| `REQUEST_MEAL` | `OK <g>`, `REFUSED <raison>` ou `ERROR <raison>` |
| `STATUS`       | `meals=<n>/<max> last=<date\|never> allowed=<yes\|no raison>` |

## À faire

- `motor_gpio.c` quand le moteur sera choisi (`make MOTOR=gpio`).
- Sur le Pi : activer NTP dans Buildroot. Sans horloge synchronisée,
  feederd refuse tout repas (le Pi n'a pas de pile RTC).
