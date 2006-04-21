# Plugin for the Ruby IRC bot (http://linuxbrit.co.uk/rbot/)
#
# A trivia quiz game.
#
# (c) 2006 Mark Kretschmann <markey@web.de>
# Licensed under GPL V2.


# Class for storing question/answer pairs
QuizBundle = Struct.new( "QuizBundle", :question, :answer )

# Class for storing player stats
PlayerStats = Struct.new( "PlayerStats", :score )


#######################################################################
# CLASS Quiz
# One Quiz instance per channel, contains channel specific data
#######################################################################
class Quiz
    attr_accessor :questions, :question, :answer, :answer_core, :hint, :hintrange

    def initialize()
        @questions = Array.new
        @question = nil
        @answer = nil
        @answer_core = nil
        @hint = nil
        @hintrange = nil
    end
end


#######################################################################
# CLASS QuizPlugin
#######################################################################
class QuizPlugin < Plugin
    def initialize()
        super

        @questions = Array.new
        @quizzes = Hash.new
    end


    def fetch_data( m )
        @bot.say( m.replyto, "Fetching questions from server.." )
        data = ""

        begin
            data = @bot.httputil.get( URI.parse( "http://amarok.kde.org/amarokwiki/index.php/Rbot_Quiz" ) )
            @bot.say( m.replyto, "done." )
        rescue
            @bot.say( m.replyto, "Failed to connect to the server. oioi." )
            return
        end

        @questions = []
        data = data.split( "QUIZ DATA START" )[1]
        data = data.split( "QUIZ DATA END" )[0]

        entries = data.split( "</p><p><br />" )

        entries.each do |e|
            p = e.split( "</p><p>" )
            b = QuizBundle.new( p[0].chomp, p[1].chomp )
            @questions << b
        end
    end


    def shuffle( m )
        return unless @quizzes.has_key?( m.target )
        q = @quizzes[m.target]

        q.questions.clear
        temp = @questions.dup

        temp.length.times do
            i = rand( temp.length )
            q.questions << temp[i]
            temp.delete_at( i )
        end
    end


    def help( plugin, topic="" )
        "Quiz game. 'quiz' => ask a question. 'quiz hint' => get a hint. 'quiz solve' => solve this question. 'quiz score' => show your score. 'quiz top5' => show top 5 players. 'quiz stats' => show some statistics. 'quiz fetch' => fetch new questions from server.\nYou can add new questions at http://amarok.kde.org/amarokwiki/index.php/Rbot_Quiz"
    end


    def listen( m )
        return unless @quizzes.has_key?( m.target )
        q = @quizzes[m.target]

        return if q.question == nil

        message = m.message.downcase.strip

        if message == q.answer.downcase or message == q.answer_core.downcase
            replies = []
            replies << "BINGO!! #{m.sourcenick} got it right. The answer was: #{q.answer}"
            replies << "OMG!! PONIES!! #{m.sourcenick} is the cutest. The answer was: #{q.answer}"
            replies << "HUZZAAAH! #{m.sourcenick} did it again. The answer was: #{q.answer}"
            replies << "YEEEHA! Cowboy #{m.sourcenick} scored again. The answer was: #{q.answer}"
            replies << "STRIKE! #{m.sourcenick} pwned you all. The answer was: #{q.answer}"
            replies << "YAY :)) #{m.sourcenick} is totally invited to my next sleepover. The answer was: #{q.answer}"
            replies << "And the crowd GOES WILD for #{m.sourcenick}.  The answer was: #{q.answer}"
            replies << "GOOOAAALLLL! That was one fine strike by #{m.sourcenick}. The answer was: #{q.answer}"
            replies << "#{m.sourcenick} deserves a medal! Only #{m.sourcenick} could have known the answer was: #{q.answer}"
            replies << "Okay, #{m.sourcenick} is officially a spermatologist! Answer was: #{q.answer}"
            replies << "I bet that #{m.sourcenick} knows where the word 'trivia' comes from too! Answer was: #{q.answer}"

            @bot.say( m.replyto, replies[rand( replies.length )] )

            stats = nil
            if @registry.has_key?( m.sourcenick )
                stats = @registry[m.sourcenick]
            else
                stats = PlayerStats.new( 0 )
                puts( "NEW PLAYER" )
            end

            stats["score"] = stats.score + 1
            @registry[m.sourcenick] = stats

            q.question = nil
        end
    end

    #######################################################################
    # Command handling
    #######################################################################
    def cmd_quiz( m, params )
        fetch_data( m ) if @questions.empty?

        unless @quizzes.has_key?( m.target )
            @quizzes[m.target] = Quiz.new
        end
        q = @quizzes[m.target]

        unless q.question == nil
            @bot.say( m.replyto, "#{m.sourcenick}: Answer the current question first!" )
            return
        end

        shuffle( m ) if q.questions.empty?

        i = rand( q.questions.length )
        q.question    = q.questions[i].question
        q.answer      = q.questions[i].answer.gsub( "#", "" )
        begin
            q.answer_core = /(#)(.*)(#)/.match( q.questions[i].answer )[2]
        rescue
            q.answer_core = nil
        end
        q.answer_core = q.answer.dup if q.answer_core == nil

        q.questions.delete_at( i )

        q.hint = ""
        (0..q.answer_core.length-1).each do |index|
            if q.answer_core[index, 1] == " "
                q.hint << " "
            else
                q.hint << "."
            end
        end

        # Generate array of unique random range
        q.hintrange = (0..q.answer_core.length-1).sort_by{rand}

        @bot.say( m.replyto, q.question )
    end


    def cmd_solve( m, params )
        return unless @quizzes.has_key?( m.target )
        q = @quizzes[m.target]

        @bot.say( m.replyto, "The correct answer was: #{q.answer}" )

        q.question = nil
    end


    def cmd_hint( m, params )
        return unless @quizzes.has_key?( m.target )
        q = @quizzes[m.target]

        if q.question == nil
            @bot.say( m.replyto, "Get a question first!" )
        else
            if q.hintrange.length >= 5
                # Reveal two characters
                2.times do
                    index = q.hintrange.pop
                    q.hint[index] = q.answer_core[index]
                end
                @bot.say( m.replyto, "Hint: #{q.hint}" )
            elsif q.hintrange.length >= 1
                # Reveal one character
                index = q.hintrange.pop
                q.hint[index] = q.answer_core[index]
                @bot.say( m.replyto, "Hint: #{q.hint}" )
            else
                # Bust
                @bot.say( m.replyto, "You lazy bum, what more can you want?" )
            end
        end
    end


    def cmd_fetch( m, params )
        fetch_data( m )
        shuffle( m )
    end


    def cmd_top5( m, params )
        @bot.say( m.replyto, "* Top 5 Players:" )

        # Convert registry to array, then sort by score
        players = @registry.to_a.sort { |a,b| a[1].score<=>b[1].score }

        1.upto( 5 ) do |i|
            player = players.pop
            nick = player[0]
            score = player[1].score
            @bot.say( m.replyto, "  #{i}. #{nick} (#{score})" )
        end
    end


    def cmd_stats( m, params )
        fetch_data( m ) if @questions.empty?

        @bot.say( m.replyto, "* Total Number of Questions:" )
        @bot.say( m.replyto, "  #{@questions.length}" )
    end


    def cmd_score( m, params )
        if @registry.has_key?( m.sourcenick )
            score = @registry[m.sourcenick].score
            @bot.say( m.replyto, "#{m.sourcenick}: Your score is: #{score}" )
        else
            @bot.say( m.replyto, "#{m.sourcenick}: You don't have a score yet, lamer." )
        end
    end


    def cmd_score_player( m, params )
        if @registry.has_key?( params[:player] )
            score = @registry[params[:player]].score
            @bot.say( m.replyto, "#{params[:player]}'s score is: #{score}" )
        else
            @bot.say( m.replyto, "#{params[:player]} does not have a score yet. Lamer." )
        end
    end
end



plugin = QuizPlugin.new

plugin.map 'quiz',               :action => 'cmd_quiz'
plugin.map 'quiz solve',         :action => 'cmd_solve'
plugin.map 'quiz hint',          :action => 'cmd_hint'
plugin.map 'quiz score',         :action => 'cmd_score'
plugin.map 'quiz score :player', :action => 'cmd_score_player'
plugin.map 'quiz fetch',         :action => 'cmd_fetch'
plugin.map 'quiz top5',          :action => 'cmd_top5'
plugin.map 'quiz stats',         :action => 'cmd_stats'

