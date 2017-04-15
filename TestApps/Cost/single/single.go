package main

import (
	"bufio"
	"encoding/gob"
	"fmt"
	"log"
	"os"
	"path"
	"runtime/pprof"
	"sort"
	"strconv"
	"time"
)

const DAMP = 0.85

var (
	itt         = 20
	tests       = 1
	profile     = false
	updateCache = false
)

type page struct {
	Incomming_rank float32
	edges          []int
}

type Page2 struct {
	Incomming_rank float32
	Edge_offset    int
	Num_edges      int
}

func newPage(pid int) *page {
	return &page{Incomming_rank: 0.0, edges: make([]int, 0)}
}

func (p *page) String() string {
	var s string
	s += fmt.Sprintf("I-Rank: %f ", p.Incomming_rank)
	s += "Edges: ["
	for _, e := range p.edges {
		s += fmt.Sprintf("%d,", e)
	}
	s += "] "
	return s
}

func printPages() {
	for i, id := range ids {
		fmt.Printf("Page: %d,", id)
		fmt.Printf("Num_Edges: %d,", apages[id].Num_edges)
		fmt.Printf("Edge_Offset: %d,", apages[id].Edge_offset)
		fmt.Printf("Rank: %f,", rank[i])
		fmt.Printf("Incomming-Rank: %f,", apages[id].Incomming_rank)
		fmt.Printf("EdgeNorm: %f,", edgenorm[i])
		fmt.Print("Edges: [")
		for j := 0; j < apages[id].Num_edges; j++ {
			fmt.Printf("%d,", edges[apages[id].Edge_offset+j])
		}
		fmt.Println("]")
	}
}

/*
func (p Page2) String() string {
	var s string
	s += fmt.Sprintf("I-Rank: %f ", p.Incomming_rank)
	s += "Edges: ["
	for _, e := range p.edges {
		s += fmt.Sprintf("%d,", e)
	}
	s += "] "
	return s
}*/

func (p Page2) MemoryString() string {
	var s string
	s += fmt.Sprintf("---------page----------\n")
	s += fmt.Sprintf("---------rank----------\n")
	s += fmt.Sprintf("%+v\n", &p.Incomming_rank)
	s += fmt.Sprintf("---offset----#edges----\n")
	s += fmt.Sprintf("%d-%d\n", p.Edge_offset, p.Num_edges)
	s += fmt.Sprintf("---------edges----------\n")
	for i := 0; i < p.Num_edges; i++ {
		s += fmt.Sprintf("%+v\n", &(edges[p.Edge_offset+i]))
	}
	s += "]\n"
	return s
}

var pages map[int]*page

//apages is a memory map for vertex id's and is potentially sparse
//index i of apages corresponds to the id of the vertex it represents
//It is similar to a hash
var apages []Page2
var ids []int
var edgenorm []float32
var rank []float32
var edges []int

func main() {
	graphFile := os.Args[1]
	if profile {
		f, err := os.Create("cpu.prof")
		if err != nil {
			log.Fatal("could not create CPU profile: ", err)
		}
		if err := pprof.StartCPUProfile(f); err != nil {
			log.Fatal("could not start CPU profile: ", err)
		}
		defer pprof.StopCPUProfile()
	}
	pages = make(map[int]*page, 0)
	start := time.Now()
	parseFile(graphFile)
	dir := time.Now().Sub(start)
	fmt.Printf("Parse Duration %f\n", dir.Seconds())
	//parseFile("../tiny.txt")
	//fmt.Println("parsed")
	total := 0.0
	for i := 0; i < tests; i++ {
		start := time.Now()
		//pageRank2(itt)
		pageRank3(itt)
		dir := time.Now().Sub(start)
		fmt.Printf("Rounds %d Duration %f\n", itt, dir.Seconds())
		total += dir.Seconds()
	}
	fmt.Printf("Average time %f\n", total/float64(tests))

	//for i := range pages {
	//	fmt.Print((*pages[i]).String())
	//}
}
func pageRank3(rounds int) {
	var outrank float32
	var alpha = (1 - DAMP) / float32(len(pages))
	//var count int64
	for i := 0; i < rounds; i++ {
		//printPages()
		for j := 0; j < len(ids); j++ {
			outrank = rank[j] / edgenorm[j]
			//fmt.Printf("page %d\n", ids[j])
			for k := 0; k < apages[ids[j]].Num_edges; k++ {
				//send message to another page
				apages[edges[apages[ids[j]].Edge_offset+k]].Incomming_rank += outrank
				//fmt.Printf("page: %d edge %d\n", ids[j], edges[apages[ids[j]].Edge_offset+k])
				//count++
			}
		}
		//fmt.Println(count)
		//count = 0
		for j := 0; j < len(ids); j++ {
			rank[j] = alpha + (DAMP * apages[ids[j]].Incomming_rank)
			//apages[j].Incomming_rank[j] = 0.0
		}
		//printPages()
	}
}

/*
func pageRank2(rounds int) {
	var outrank float32
	var alpha = (1 - DAMP) * (1.0 / float32(len(pages)))
	for i := 0; i < rounds; i++ {
		for j := 0; j < len(ids); j++ {
			outrank = rank[j] / edgenorm[j]

			for k := 0; k < len(apages[j].edges); k++ {
				apages[apages[j].edges[k]].Incomming_rank += outrank
			}
		}
		for j := 0; j < len(ids); j++ {
			rank[j] = alpha + (DAMP * apages[j].Incomming_rank)
		}
	}
}*/
/*
func pageRank(rounds int) {
	for i := 0; i < rounds; i++ {
		for j := 0; j < len(ids); j++ {
			sendMessages(ids[j])
		}
		for j := 0; j < len(ids); j++ {
			update(ids[j])
		}
	}
}

func sendMessages(id int) {
	var outrank float32
	var p Page2
	p = apages[id]
	outrank = rank[id] / float32((p.Num_edges + 1))
	for i := 0; i < p.Num_edges; i++ {
		apages[p.edges[i]].Incomming_rank += outrank
	}
}

func update(id int) {
	apages[id].rank = (1-DAMP)*(1.0/float32(len(pages))) + (DAMP * apages[id].Incomming_rank)
}
*/

func parseFile(filename string) {
	base := path.Base(filename)
	if !updateCache && fileCached(base) {
		readCache(base)
		return
	}
	fmt.Println("reading from file")
	f, err := os.Open(filename)
	if err != nil {
		log.Fatalf("%s", err.Error)
	}
	defer f.Close()
	scanner := bufio.NewScanner(f)

	var i int
	var buf string
	var max int
	var e int
	for scanner.Scan() {
		buf = scanner.Text()
		//fmt.Println(buf)
		for i = 0; i < len(buf); i++ {
			if buf[i] == '\t' {
				a, err := strconv.Atoi(buf[0:i])
				if err != nil {
					log.Fatal(err)
				}
				b, err := strconv.Atoi(buf[i+1 : len(buf)])
				if err != nil {
					log.Fatal(err)
				}
				//fmt.Printf("%d -> %d\n", a, b)
				if _, ok := pages[a]; !ok {
					pages[a] = newPage(a)
				}
				if _, ok := pages[b]; !ok {
					pages[b] = newPage(b)
				}
				if a > max {
					max = a
				}
				if b > max {
					max = b
				}
				pages[a].edges = append(pages[a].edges, b)
				pages[b].Incomming_rank = 0.0
				e++
			}
		}
	}
	ids = make([]int, len(pages))
	//apages and edges are the main structures
	apages = make([]Page2, max+1)
	edges = make([]int, e)
	//edgenorm and rank are optimizations
	edgenorm = make([]float32, len(pages))
	rank = make([]float32, len(pages))
	j := 0 // id index
	k := 0 //edge index
	for i := range pages {
		ids[j] = i //id[index] = vertex-id
		j++
	}
	sort.Ints(ids)
	for i, id := range ids {
		apages[id].Num_edges = len(pages[id].edges)
		//fmt.Printf("#edges %d %d", id, len(pages[id].edges))
		rank[i] = 1.0
		edgenorm[i] = float32(apages[id].Num_edges + 1)
		apages[id].Edge_offset = k
		for l := range pages[id].edges {
			edges[k] = pages[id].edges[l]
			k++
		}
	}

	WriteToFiles(base)

	//for i := range apages {
	//	fmt.Println(apages[i].MemoryString())
	//}

	if err := scanner.Err(); err != nil {
		log.Fatal(err)
	}
}

func WriteToFiles(filename string) {
	//make a cache
	_, err := os.Stat("./cache/")
	if os.IsNotExist(err) {
		os.Mkdir("./cache/", 0711)
	}
	//make a dir
	_, err = os.Stat("./cache/" + filename)
	if os.IsNotExist(err) {
		os.Mkdir("./cache/"+filename, 0711)
	}
	dir := "./cache/" + filename + "/"
	writeFile(dir+"apages", apages)
	writeFile(dir+"edgenorm", edgenorm)
	writeFile(dir+"ids", ids)
	writeFile(dir+"rank", rank)
	writeFile(dir+"edges", edges)
}

func writeFile(filename string, item interface{}) {
	f, err := os.Create(filename)
	if err != nil {
		log.Fatal(err)
	}
	defer f.Close()
	enc := gob.NewEncoder(f)
	err = enc.Encode(item)
	if err != nil {
		log.Fatal(err)
	}
}

func fileCached(filename string) bool {
	_, err := os.Stat("./cache/" + filename)
	if os.IsNotExist(err) {
		return false
	}
	return true
}

func readCache(filename string) {
	fmt.Println("reading from cache")
	dir := "./cache/" + filename + "/"
	readCachefile(dir+"apages", &apages)
	readCachefile(dir+"edgenorm", &edgenorm)
	readCachefile(dir+"ids", &ids)
	readCachefile(dir+"rank", &rank)
	readCachefile(dir+"edges", &edges)
}

func readCachefile(objectname string, object interface{}) {
	f, err := os.Open(objectname)
	if err != nil {
		log.Fatal(err)
	}
	dec := gob.NewDecoder(f)
	err = dec.Decode(object)
	if err != nil {
		log.Fatal(err)
	}
}

/*
type pageHash [][]*page

func NewPageHash(size int) *pageHash {
	p := make([][]*page, size)
	logsize := math.Sqrt(float32(size))
	for i := range p {
		p[i] = make([]*page, int(logsize))
	}
	return *pageHash(p)
}

func (ph *pageHash) Put(id int) {
	var l1 int
	var l2 int
*/
