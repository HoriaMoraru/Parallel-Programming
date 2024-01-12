->Algoritmul Marching Squares Paralelizat
Descriere Generală
Acest depozit conține o implementare paralelizată a algoritmului marching squares,
proiectată pentru a genera hărți de contur eficient pentru imagini.
Algoritmul a fost paralelizat folosind pthreads, permițând o performanță îmbunătățită pe sisteme cu mai multe nuclee.

->Descrierea Algoritmului
Algoritmul marching squares constă în următoarele etape:

Redimensionare: Dacă este necesar, imaginea de intrare este redimensionată la o rezoluție specificată.
Inițializare Hartă de Contur: Un set de hărți de contur este inițializat pe baza unor configurații predefinite.
Redimensionare Paralelizată: Imaginea este redimensionată în paralel folosind pthreads pentru a îmbunătăți eficiența.
Eșantionare Grilă: O grilă este creată prin eșantionarea imaginii redimensionate, determinând dacă fiecare punct depășește o valoare prag.
Marching Squares: Algoritmul identifică tipul de contur pentru fiecare subgrilă și înlocuiește pixelii din imaginea originală cu cei corespunzători conturului.
Ieșire: Imaginea finală redimensionată și conturată este scrisă în fișierul specificat.

Detalii de Paralelizare
Algoritmul este paralelizat folosind pthreads pentru a distribui sarcinile de lucru între mai multe fire de execuție.
Fiecare fir de execuție este responsabil pentru procesarea unei părți specifice a imaginii în timpul fazei de redimensionare, de creeare
a sample-grid-ului si de faza de marching.
Acest lucru a fost posibil prin calcularea unor variable start si end care specifica punctul din care fiecare pthread trece sa inceapa
sa redimensioneze imaginea.
De asemenea, am creeat 2 structuri in care retin informatiile generale ale pthread-urilor, si una specifica fiecarui pthread in care
retin ID-ul , start-ul si end-ul pentru redimensionare.
