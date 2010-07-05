
scoreboardgen_output_file = [www/@(month (now))-scoreboard.html]

if (! (symbol? statsdb)) [
    log_error ['@FILENAME' script failed because stats are disabled.]
    throw runtime.script_failure
]

if (! (symbol? scoreboardgen_writer) ) [
    logfile $scoreboardgen_output_file scoreboardgen_writer
]

clearfile $scoreboardgen_output_file

scoreboardgen_html_tblhdrs = [
    <th>Name</th>
    <th>Flags Scored</th>
    <th>Flags Defended</th>
    <th>Frags Record</th>
    <th>Total Frags</th>
    <th>Total Deaths</th>
    <th>Accuracy (%)</th>
    <th>Kpd</th>
]

scoreboardgen_html_tbldata = "    "

statsdb eval [select name,
                    sum(scored) as TotalScored,
                    sum(defended) as TotalDefended,
                    max(frags) as MostFrags,
                    sum(frags) as TotalFrags,
                    sum(deaths) as TotalDeaths,
                    round((0.0+sum(hits))/(sum(hits)+sum(misses))*100) as Accuracy,
                    round((0.0+sum(frags))/sum(deaths),2) as Kpd
                from players 
    inner join matches on players.match_id=matches.id
    inner join ctfplayers on players.id=ctfplayers.player_id
    where matches.datetime > date("now","start of year") group by name order by TotalScored desc limit 100] [

    local td [ 
        parameters colname
        result [<td>@(column $colname)</td>]
    ]
    
    scoreboardgen_html_tbldata = (concat $scoreboardgen_html_tbldata [
        <tr onmouseover="this.className='highlight'" onmouseout="this.className=''">
            @(td name) 
            @(td TotalScored)
            @(td TotalDefended)
            @(td MostFrags)
            @(td TotalFrags)
            @(td TotalDeaths)
            @(td Accuracy)
            @(td Kpd)
        </tr>
    ])
]

try do [
    scoreboardgen_output = [@@(readfile share/scoreboard.html.tpl)]
] [
    log_error ['@FILENAME' script failed because the scoreboard template contains errors.]
    throw runtime.script_failure
]

scoreboardgen_writer $scoreboardgen_output
close_logfile scoreboardgen_writer