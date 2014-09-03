task :default => :build

ELF_NAME = 'end'
UW_HOME  = ENV['UW_HOME']
UW_USER  = ENV['UW_USER']

def check_code_name
  return if File.exists?('CODE_NAME')
  File.open('CODE_NAME', 'w') do |fd|
    fd.puts `whoami`.chomp
  end
end

def increment_version
  version = if File.exists?('VERSION')
              File.read('VERSION').to_i + 1
            else
              0
            end
  File.open('VERSION', 'w') do |fd|
    fd.puts version
  end
end

def rsync_command
  "rsync -trulip --exclude '.git/' --exclude './measurement' " +
    "./ uw:#{UW_HOME}/trains"
end

def path_to dest
  paths = case dest
          when :future
            ['/u3/marada/arm-eabi-gcc/bin',
             '/u3/marada/arm-eabi-gcc/libexec/gcc/arm-eabi/4.9.0']
          when :clang
            ['/u/cs444/bin',
             '/u3/marada/arm-eabi-gcc/bin',
             '/u3/marada/arm-eabi-gcc/libexec/gcc/arm-eabi/4.9.0']
          when :cowan
            ['/u/wbcowan/gnuarm-4.0.2/libexec/gcc/arm-elf/4.0.2',
             '/u/wbcowan/gnuarm-4.0.2/arm-elf/bin']
          end
  "export PATH=#{paths.join(':')}:$PATH"
end

desc 'Build ferOS in the CS environment'
task :build, :params do |_, args|
  params = args[:params] || 'psad'

  env  = []

  # individual flags
  env << 'STRICT=yes'           if params.include? 's'
  env << 'BENCHMARK=yes'        if params.include? 'b'
  env << 'ASSERT=yes'           if params.include? 'a'
  env << 'DEBUG=yes'            if params.include? 'd'
  env << 'RELEASE=1'            if params.include? '1'
  env << 'RELEASE=2'            if params.include? '2'
  env << 'RELEASE=3'            if params.include? '3'
  env << 'RELEASE=s'            if params.include? 'x'
  env << 'RELEASE=g'            if params.include? 'g'

  cmds = if params.include? 't'
           []
         else
           ['cd trains/']
         end

  if params.include? 'f'
    env  << 'FUTURE=zomg'
    cmds << path_to(:future)
  elsif params.include? 'l'
    env  << 'CLANG=yes'
    cmds << path_to(:clang)
  else
    cmds << path_to(:cowan)
  end

  env  = env.join ' '
  cmds = cmds.reverse.join ' && '
  jobs = params.include?('p') ? '-j' : '' # parallelize build

  cmds << " && echo Cleaning Environment && make clean && touch Makefile"
  cmds << " && echo Building Fresh Kernel && #{env} make -s #{jobs}"

  if params.include? 't'
    sh cmds
  else
    # we also want to copy the thing to the TFTP directory
    cmds << " && echo Copying kernel to TFTP server"
    cmds << " && cp kernel.elf /u/cs452/tftp/ARM/#{UW_USER}/#{ELF_NAME}.elf"

    increment_version
    check_code_name
    sh rsync_command
    sh "ssh uw '#{cmds}'"
  end
end

desc 'Debug build with benchmarks'
task(:bench) { Rake::Task[:build].invoke('psadb') }

desc 'Standard release build'
task(:release) { Rake::Task[:build].invoke('p2s') }

desc 'Standard release build with benchmarking enabled'
task(:release_bench) { Rake::Task[:build].invoke('p2sb') }

desc 'Build with clang'
task(:clang) { Rake::Task[:build].invoke('l2b') }

desc 'Build using GCC from the future'
task(:future) { Rake::Task[:build].invoke('f2b') }

desc 'Make a local release_bench build'
task(:local) { Rake::Task[:build].invoke('p2sbt') }

desc 'Generate the PDF guide'
task :guide => 'guide:pdf'

namespace :guide do
  desc 'Generate MD5 hashes of project files'
  task :md5 do
    require 'digest/md5'
    require 'csv'

    md5 = Digest::MD5.new

    [
     ['headers', FileList['include/**/*.h']],
     ['impls',   FileList['src/**/*.c'] + FileList['src/**/*.asm']],
     ['others',  ['README.md', 'Makefile', 'Rakefile'] + FileList['ld/*.ld']]
    ].each do |set, list|
      CSV.open("guide/md5_info_#{set}.csv", 'w') do |csv|
        csv << ['file', 'hash']
          list.sort.each do |file|
          csv << [file.gsub(/_/, '\_'), md5.hexdigest(File.read file)]
        end
      end

      puts "md5sum -> guide/md5_info_#{set}.csv"
    end
  end

  desc 'Generate the tasks diagram'
  task :dot do
    sh 'dot -Tpng guide/tasks.dot > guide/tasks.png'
    sh 'open guide/tasks.png'
  end

  desc 'Compile the PDF guide'
  task :pdf => [:md5, :dot] do
    cd 'guide/' do
      sh 'pdflatex guide.tex'
      sh 'open guide.pdf'
    end
  end
end
